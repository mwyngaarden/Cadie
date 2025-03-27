#include <algorithm>
#include <array>
#include <mutex>
#include <sstream>
#include <thread>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include "bench.h"
#include "search.h"
#include "eval.h"
#include "gen.h"
#include "history.h"
#include "move.h"
#include "pos.h"
#include "order.h"
#include "tt.h"

using namespace std;

atomic_bool Searching = false;
atomic_bool StopRequest = false;

SearchInfo      si;
SearchLimits    sl;
MoveStack       mstack;
KeyStack        kstack;

static History history;
static u8 LMReductions[64][64];
static u8 LMPruning[2][9];
static i16 Evals[PliesMax];

static int qsearch(Position& pos, int alfa, const int beta, const int ply, const int depth, PV& pv, bool is_pv);
static int  search(Position& pos, int alfa,       int beta, const int ply, const int depth, PV& pv, Move skip_move = Move::None());

static int search_aspirate(Position pos, int depth, PV& pv, int score);
static void search_iterate();

static string pv_string(PV& pv);
static u8 calc_bound(int score, int alfa, int beta);
static void pv_append(PV& dst, const PV& src);
static int calc_reductions(const Node& node, int depth, Move m, Order& order, bool checks);

int search(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv, Move skip_move)
{
    if (depth <= 0)
        return qsearch(pos, alfa, beta, ply, 0, pv, beta - alfa > 1);

    // Do we need to abort the search?
    si.checkup();

    Node node(pos, alfa, beta);

    if (node.pv_node)
        si.sel_depth = max(si.sel_depth, ply + 1);

    pv[0] = Move::None();

    if (ply && !skip_move) {
        if (pos.draw())
            return ScoreDraw;

        if (ply >= PliesMax)
            return pos.checkers() ? ScoreDraw : eval(pos);

        // mate distance pruning

        alfa = max(mated_in(ply), alfa);
        beta = min(mate_in(ply + 1), beta);

        if (alfa >= beta) return alfa;
    }

    node.tthit = !skip_move && ttable.get(node.tte, pos.key(), ply);

    if (node.tthit) {
        if (node.tte.depth >= depth && !node.pv_node) {
            bool cut =  node.tte.bound == BoundExact
                    || (node.tte.bound == BoundLower && node.tte.score >= beta)
                    || (node.tte.bound == BoundUpper && node.tte.score <= alfa);

            if (cut) return node.tte.score;
        }

        if (   SingularExt
            && ply
            && ply < si.root_depth * 2
            && depth >= SEDepthMin
            && score_is_eval(node.tte.score)
            && (node.tte.bound & BoundLower)
            && node.tte.depth >= depth - SEDepthOffset)
            node.ext_move = node.tte.move;
    }

    if (pos.checkers())
        Evals[ply] = node.eval = mated_in(ply);
    else if (node.tthit) {
        Evals[ply] = node.eval = node.tte.eval;

        node.eval = pos.draw_scale(node.eval);

        bool adjust =  node.tte.bound == BoundExact
                   || (node.tte.bound == BoundLower && node.eval < node.tte.score)
                   || (node.tte.bound == BoundUpper && node.eval > node.tte.score);

        if (adjust) node.eval = node.tte.score;
    }
    else {
        if (skip_move)
            node.eval = Evals[ply];
        else {
            if (ply <= 1 || mstack.back() != Move::Null())
                node.eval = eval(pos);
            else
                node.eval = -Evals[ply - 1] + 2 * TempoB;

            Evals[ply] = node.eval;
        }

        node.eval = pos.draw_scale(node.eval);
    }

    node.improving = Evals[ply] > (ply < 2 ? 0 : Evals[ply - 2]);

    if (!ply)
        node.tte.move = si.best_move;

    history.reset_killers(pos.side(), ply + 2);

    if (!pos.checkers() && !node.pv_node && !skip_move) {

        if (   StaticNMP
            && depth <= StaticNMPDepthMax
            && pos.pieces(pos.side())
            && node.eval >= beta + (depth - node.improving) * StaticNMPFactor)
            return node.eval;

        if (   NMPruning
            && depth >= NMPruningDepthMin
            && mstack.back(0) != Move::Null()
            && mstack.back(1) != Move::Null()
            && score_is_eval(beta)
            && node.eval >= beta
            && pos.pieces(pos.side()))
        {
            int R = 3 + depth / 3;

            UndoInfo undo = pos.undo_info();

            pos.make_null();
            int score = -search(pos, -beta, -beta + 1, ply + 1, depth - R, node.pv);
            pos.unmake_null(undo);

            if (score >= beta)
                return !score_is_eval(score) ? beta : score;
        }

        int ubound = beta + 175;

        if (   depth >= 5
            && score_is_eval(beta)
            && !(node.tthit && node.tte.depth >= depth - 3 && node.tte.score < ubound))
        {
            Order order(pos, node.tte.move);

            UndoInfo undo = pos.undo_info();

            for (Move m = order.next(); m; m = order.next()) {
                if (!pos.move_is_legal(m))
                    continue;

                pos.make_move(m);

                ttable.prefetch(pos.key());
                etable.prefetch(pos.key());

                int score = -qsearch(pos, -ubound, -ubound + 1, ply + 1, 0, node.pv, false);

                if (score >= ubound)
                    score = -search(pos, -ubound, -ubound + 1, ply + 1, depth - 4, node.pv);

                pos.unmake_move(undo);

                if (score >= ubound)
                    return score;
            }
        }
    }

    MoveList quiets;

    Order order(pos, history, node.tte.move, ply, depth);

    UndoInfo undo = pos.undo_info();

    int see_quiet = -150 * (depth - 7) * (depth - 7);
    int see_noisy = -150 * (depth - 3);
    int lmp_limit = depth <= 8 ? LMPruning[node.improving][depth] : 0;

    for (Move m = order.next(); m; m = order.next()) {
        if (!pos.move_is_legal(m))
            continue;

        node.legals++;

        if (m == skip_move)
            continue;

        bool checks     = pos.move_is_check(m);
        bool quiet      = !checks && !m.is_tactical();
        bool dangerous  = pos.checkers() || !quiet;

        if (node.best_score >= -4000 && order.score() < Order::ScoreCounter) {
            if (ply && depth <= 8 && !dangerous && node.legals >= lmp_limit)
                continue;

            if (depth >= 8 && !dangerous && !pos.see(m, see_quiet))
                continue;

            if (depth <= 7 && !dangerous && !order.see())
                continue;

            if (depth >= 4 && !quiet && !pos.see(m, see_noisy))
                continue;

            if (depth <= 3 && !quiet && !order.see())
                continue;
        }

        int next_depth = depth - 1;

        if (order.singular())
            next_depth++;
        else if (m == node.ext_move) {
            int lbound = node.tte.score - 2 * depth;

            int score = search(pos, lbound - 1, lbound, ply, (depth - 1) / 2, node.pv, m);

            if (score < lbound)
                next_depth++;
            else if (lbound >= beta)
                return lbound;
        }

        int red;

        if (depth < 3 || node.legals < 2)
            red = 0;
        else {
            red = calc_reductions(node, depth, m, order, checks);
            red = clamp(red, 0, next_depth - 1);
        }

        // UCI info
        if (!ply) {
            si.curr_move     = m;
            si.curr_move_num = node.legals;
        }

        if (!m.is_tactical())
            quiets.add(m);

        pos.make_move(m);

        ttable.prefetch(pos.key());
        etable.prefetch(pos.key());

        int score, zws = (node.pv_node && node.legals > 1) || red;

        if (zws)
            score = -search(pos, -alfa - 1, -alfa, ply + 1, next_depth - red, node.pv);

        if (!zws || score > alfa)
            score = -search(pos, -beta, -alfa, ply + 1, next_depth, node.pv);

        pos.unmake_move(undo);

        if (score > node.best_score) {
            node.best_score = score;

            if (!ply && node.legals == 1)
                si.fail_low = node.best_score <= alfa;

            if (node.best_score > alfa) {
                alfa = node.best_score;
                node.best_move = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, node.pv);

                    if (!ply && node.legals > 1 && depth > 1)
                        si.update(depth, node.best_score, pv, false);
                }

                if (alfa >= beta) {
                    if (!skip_move && !node.best_move.is_tactical())
                        history.update(pos, ply, depth, node.best_move, quiets);

                    break;
                }
            }
        }
    }

    if (!node.legals)
        return pos.checkers() ? mated_in(ply) : ScoreDraw;

    if (node.best_score <= -ScoreMate)
        return max(alfa, mated_in(ply + 1));

    if (!skip_move) {
        i16 eval = pos.checkers() ? -ScoreMate : Evals[ply];
        u8 bound = calc_bound(node.best_score, node.orig_alfa, beta);

        ttable.set(pos.key(), node.best_move, node.best_score, eval, depth, bound, ply);
    }

    return node.best_score;
}

int qsearch(Position& pos, int alfa, const int beta, const int ply, const int depth, PV& pv, bool is_pv)
{
    // Do we need to abort the search?
    si.checkup();

    Node node(pos, alfa, beta);

    int adj_eval = -ScoreMate;

    if (is_pv) {
        pv[0] = Move::None();

        si.sel_depth = max(si.sel_depth, ply + 1);
    }

    if (ply) {
        if (pos.draw())
            return ScoreDraw;

        if (ply >= PliesMax)
            return pos.checkers() ? ScoreDraw : eval(pos);
    }

    node.tthit = ttable.get(node.tte, pos.key(), ply);

    if (node.tthit && node.tte.depth >= depth && !is_pv) {
        bool cut =  node.tte.bound == BoundExact
                || (node.tte.bound == BoundLower && node.tte.score >= beta)
                || (node.tte.bound == BoundUpper && node.tte.score <= alfa);

        if (cut) return node.tte.score;
    }

    if (pos.checkers())
        node.eval = adj_eval = mated_in(ply);
    else {
        if (node.tthit) {
            node.eval = node.tte.eval;

            adj_eval = pos.draw_scale(node.eval);

            bool adjust =  node.tte.bound == BoundExact
                       || (node.tte.bound == BoundLower && adj_eval < node.tte.score)
                       || (node.tte.bound == BoundUpper && adj_eval > node.tte.score);

            if (adjust) adj_eval = node.tte.score;
        }
        else {
            if (depth < 0 || mstack.back() != Move::Null())
                node.eval = eval(pos);
            else
                node.eval = -Evals[ply - 1] + 2 * TempoB;

            adj_eval = pos.draw_scale(node.eval);
        }

        if (adj_eval >= beta)
            return adj_eval;

        if (adj_eval > alfa)
            alfa = adj_eval;
    }

    node.best_score = adj_eval;

    size_t qevasions = 0;

    Order order(pos, history, node.tte.move, ply, depth);

    UndoInfo undo = pos.undo_info();

    for (Move m = order.next(); m; m = order.next()) {
        if (!pos.move_is_legal(m))
            continue;

        node.legals++;

        if (node.best_score >= -4000 && qevasions >= 1)
            break;

        if (!pos.checkers() && depth <= DepthMin && !pos.move_is_recap(m))
            continue;

        pos.make_move(m);

        ttable.prefetch(pos.key());
        etable.prefetch(pos.key());

        int score = -qsearch(pos, -beta, -alfa, ply + 1, depth - 1, node.pv, is_pv);

        pos.unmake_move(undo);

        qevasions += !m.is_tactical() && pos.checkers();

        if (score > node.best_score) {
            node.best_score = score;

            if (score > alfa) {
                alfa = score;
                node.best_move = m;

                if (is_pv) {
                    pv[0] = m;

                    pv_append(pv, node.pv);
                }

                if (alfa >= beta) break;
            }
        }
    }

    if (pos.checkers() && !node.legals)
        return mated_in(ply);

    if (node.best_score <= -ScoreMate)
        return max(alfa, mated_in(ply + 1));

    if (depth == 0) {
        u8 bound = calc_bound(node.best_score, node.orig_alfa, beta);

        ttable.set(pos.key(), node.best_move, node.best_score, node.eval, 0, bound, ply);
    }

    return node.best_score;
}

static int search_aspirate(Position pos, int depth, PV& pv, int score)
{
    si.sel_depth  = 0;
    si.root_depth = depth;

    if (depth <= 5)
        return search(pos, -ScoreMate, ScoreMate, 0, depth, pv);

    int delta = 10;
    int alfa  = max(score - delta, -ScoreMate);
    int beta  = min(score + delta,  ScoreMate);

    while (true) {
        if (alfa < -4000) alfa = -ScoreMate;
        if (beta >  4000) beta =  ScoreMate;

        score = search(pos, alfa, beta, 0, depth, pv);

        if (score <= alfa) {
            beta = (alfa + beta) / 2;
            alfa = max(alfa - delta, -ScoreMate);

            depth = si.root_depth;
        } 
        else if (score >= beta) {
            beta = min(beta + delta, ScoreMate);

            if (score_is_eval(score))
                depth = max(1, depth - 1);
        } 
        else
            return score;

        delta += delta / 2;
    }
}

void search_iterate()
{
    MoveList moves;

    gen_moves(moves, si.pos, GenMode::Legal);

    si.singular = moves.size() == 1;

    PV pv;

    int score = 0;

    for (int depth = 1; depth <= DepthMax; depth++) {

        try {
            score = search_aspirate(si.pos, depth, pv, score);
        } catch (int i) {
            break;
        }

        // Aspiration completed successfully without reaching time limit or node limit

        si.update(depth, score, pv);

        if (sl.depth && depth >= sl.depth)
            break;

        if (sl.time_managed()) {
            if (si.singular)
                break;

            double usage = si.time_usage(sl);

            if (usage >= 0.7 && !si.score_drop && si.bm_stable >= 10)
                break;

            if (usage >= 0.9 && !si.score_drop)
                break;
        }
    }

    ttable.age();
}

void search_start()
{
    Searching = true;

    NMPruning           = opt_list.get("NMPruning").check_value();
    NMPruningDepthMin   = opt_list.get("NMPruningDepthMin").spin_value();

    SingularExt         = opt_list.get("SingularExt").check_value();
    SEDepthMin          = opt_list.get("SEDepthMin").spin_value();
    SEDepthOffset       = opt_list.get("SEDepthOffset").spin_value();

    StaticNMP           = opt_list.get("StaticNMP").check_value();
    StaticNMPDepthMax   = opt_list.get("StaticNMPDepthMax").spin_value();
    StaticNMPFactor     = opt_list.get("StaticNMPFactor").spin_value();

    // No reason to search if there are no legal moves
    if (GenState state = gen_state(si.pos); state != GenState::Normal) {
        gstats.num += !gstats.exc_mated;
        gstats.stalemate += state == GenState::Stalemate;
        gstats.checkmate += state == GenState::Checkmate;

        int score = state == GenState::Stalemate
                  ? ScoreDraw
                  : (si.pos.side() == White ? -ScoreMate : ScoreMate);
        
        uci_send("info depth 0 score %s", uci_score(score).c_str());
        uci_send("bestmove (none)");
    }

    else {
        gstats.stimer.start(true);

        search_iterate();

        si.uci_bestmove();

        gstats.stimer.stop(true);
        gstats.stimer.accrue(true);
    
        // Collect statistics
        
        gstats.normal++;
        gstats.num++;

        int dn = si.max_depth;

        gstats.depth_min  = min(gstats.depth_min, dn);
        gstats.depth_max  = max(gstats.depth_max, dn);
        gstats.depth_sum += dn;
        
        i64 n = si.tnodes;

        gstats.nodes_sum += n;
        gstats.nodes_min = min(gstats.nodes_min, n);
        gstats.nodes_max = max(gstats.nodes_max, n);

        i64 t = gstats.stimer.elapsed_time<Timer::Nano>();
        
        gstats.time_min = min(gstats.time_min, t);
        gstats.time_max = max(gstats.time_max, t);

        if (t > 0) {
            i64 s = n / (t / 1E+9);

            gstats.nps_min = min(gstats.nps_min, s);
            gstats.nps_max = max(gstats.nps_max, s);
        }
    }
}

void search_reset()
{
    memset(Evals, 0, sizeof(Evals));

    history.reset();

    ttable.reset();
    etable.reset();
}

bool score_is_mate(int score)
{
    return abs(score) >= ScoreMate - PliesMax;
}

bool score_is_eval(int score)
{
    return abs(score) <= 4000;
}

void SearchInfo::update(int depth, int score, PV& pv, bool complete)
{
    i64 dur = timer.elapsed_time(1);

    // Don't flood stdout if time limited
    bool report = (depth > max_depth || pv[0] != best_move)
               && (singular || !sl.time_limited() || depth >= 6 || dur >= 100);

    if (report) {
        ostringstream oss;

        oss << "info" 
            << " depth "    << depth
            << " seldepth " << sel_depth
            << " score "    << uci_score(score)
            << " time "     << dur
            << " nodes "    << tnodes
            << " nps "      << 1000 * tnodes / dur
            << " hashfull " << ttable.permille()
            << " pv "       << pv_string(pv);

        uci_send(oss.str().c_str());
    }

    max_depth = depth;

    if (pv[0] == best_move)
        bm_stable   += complete;
    else {
        bm_stable    = 0;
        bm_updates  += depth > 1;
    }

    score_drop = depth > 1 && score_prev - score >= 20;

    if (complete)
        score_prev = score;

    fail_low = false;

    best_move = pv[0];
}

void SearchInfo::uci_bestmove() const
{
    i64 dur = timer.elapsed_time();

    ostringstream oss;

    oss << "info" 
        << " depth "    << max_depth
        << " seldepth " << sel_depth
        << " time "     << dur
        << " nodes "    << tnodes;

    uci_send(oss.str().c_str());

    oss = ostringstream();

    oss << "bestmove " << si.best_move.str();

    uci_send(oss.str().c_str());
}

void SearchInfo::uci_info(i64 dur) const
{
    ostringstream oss;

    oss << "info" 
        << " depth "            << max_depth
        << " seldepth "         << sel_depth
        << " time "             << dur
        << " nodes "            << tnodes
        << " nps "              << 1000 * tnodes / dur
        << " hashfull "         << ttable.permille()
        << " currmove "         << curr_move.str()
        << " currmovenumber "   << curr_move_num;

    uci_send(oss.str().c_str());
}

// Depth limit is handled in search_iterate
void SearchInfo::checkup()
{
    if (--cnodes >= 0)
        return;

    if (!best_move.is_valid())
        return;

    i64 cnodes_next = 100000;
    i64 dur = timer.elapsed_time();

    bool abort = StopRequest.load(std::memory_order_relaxed);

    if (!abort && sl.nodes) {
        i64 diff = tnodes - sl.nodes;

        cnodes_next = -diff / 2;

        if (diff >= 0)
            abort = true;
    }

    if (!abort && sl.time_limited()) {
        i64 rem_time = 1000; // 1 second

        if (sl.move_time) {
            rem_time = sl.move_time - dur;

            if (dur >= sl.move_time)
                abort = true;
        }

        else if (sl.time_managed()) {
            i64 otime = sl.opt_time;
            i64 xtime = sl.max_time;

            double usage = dur / double(otime);

            rem_time = (dur >= otime ? xtime : otime) - dur;

            // Maximum time threshold
            if (dur >= xtime)
                abort = true;

            else if (!fail_low) {
                if (usage < 1.25 && (score_drop || curr_move_num != 1 || bm_stable == 0)) {
                    // NOP
                }

                else if (usage >= 2.0)
                    abort = true;

                else if (  usage >= 0.5
                        && !pos.checkers()
                        && !score_drop
                        && curr_move_num == 1
                        && bm_updates == 0)
                    abort = true;
 
                else if (usage >= 1.0 && curr_move_num == 1)
                    abort = true;
 
                else if (usage >= 1.0 && !score_drop && bm_stable >= 10)
                    abort = true;
 
                else if (usage >= 1.25 && (!score_drop || bm_stable >= 10))
                    abort = true;
            }
        }

        i64 n = clamp(rem_time, i64(1), i64(1000)) * 100;

        cnodes_next = min(cnodes_next, n);
    }

    if (abort)
        throw 1;

    cnodes = cnodes_next;

    if (max_depth >= 6) {
        if (dur > 5000 && dur - rep_time >= 1000) {
            rep_time = dur;

            uci_info(dur);
        }
    }
}

static string pv_string(PV& pv)
{
    ostringstream oss;

    for (size_t i = 0; pv[i] != Move::None(); i++) {
        if (i > 0) oss << ' ';

        oss << pv[i].str();
    }

    return oss.str();
}

u8 calc_bound(int score, int alfa, int beta)
{
    u8 bound = 0;

    if (score > alfa) bound |= BoundLower;
    if (score < beta) bound |= BoundUpper;

    return bound;
}

void search_init()
{
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++)
            LMReductions[d][m] = log2(d) * log2(m) * 0.4;

    for (int d = 1; d <= 8; d++) {
        double x = 3.0 + pow(d, 1.5);

        LMPruning[0][d] = x;
        LMPruning[1][d] = x * 1.25;
    }
}

void pv_append(PV& dst, const PV& src)
{
    for (int i = 0; i < PliesMax - 1; i++)
        if ((dst[i + 1] = src[i]) == Move::None())
            break;
}

int calc_reductions(const Node& node, int depth, Move m, Order& order, bool checks)
{
    int red = 0;

    if (!node.pos.checkers() && !m.is_tactical()) {
        red += LMReductions[min(depth, 63)][min(node.legals - 1, 63)];

        if (node.pv_node)
            red -= red / 2;

        int score = order.score();

        red -= clamp(score / (5 * 1024), -1, 1);

        if (checks)
            red -= 1 + node.pv_node;

        red += node.tte.move.is_valid() && node.tte.move.is_tactical();
        red += !order.see();
        red += !node.improving;
    }

    else if (!node.pv_node && !order.see()) {
        red++;

        red += !node.pos.checkers();
    }

    return red;
}
