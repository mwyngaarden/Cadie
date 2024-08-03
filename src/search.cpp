#include <algorithm>
#include <array>
#include <mutex>
#include <sstream>
#include <thread>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include "bench.h"
#include "see.h"
#include "search.h"
#include "eval.h"
#include "gen.h"
#include "history.h"
#include "move.h"
#include "pawn.h"
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
static u8 LMR[64][64];
int Evals[PliesMax];

static int qsearch(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv);
static int  search(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv, Move skip_move = MoveNone);

static void search_init(Position pos);
static int search_aspirate(Position pos, int depth, PV& pv, int score);
static void search_iterate();

static bool draw_rep(const Position& pos);
static bool draw_fifty(const Position& pos);

static string pv_string(PV& pv)
{
    ostringstream oss;

    for (size_t i = 0; pv[i] != MoveNone; i++) {
        if (i > 0) oss << ' ';

        oss << pv[i].str();
    }

    return oss.str();
}

[[maybe_unused]]
static bool pv_is_valid(Position pos, const PV& pv)
{
    for (size_t i = 0; pv[i] != MoveNone; i++) {
        MoveList moves;

        gen_moves(moves, pos, GenMode::Legal);

        if (moves.find(pv[i]) == MoveList::npos)
            return false;

        pos.make_move(pv[i]);

        moves.clear();
    }

    return true;
}

u8 calc_bound(int score, int alfa, int beta)
{
    u8 bound = 0;

    if (score < beta) bound |= BoundUpper;
    if (score > alfa) bound |= BoundLower;

    return bound;
}

static int calc_lmr(int d, int m)
{
    assert(d > 0 && m > 0);

    return log2(d) * log2(m) * 0.3 + 0.5;
}

void search_init()
{
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++)
            LMR[d][m] = calc_lmr(d, m);
}

static void pv_append(PV& dst, const PV& src)
{
    for (int i = 0; i < PliesMax - 1; i++)
        if ((dst[i + 1] = src[i]) == MoveNone)
            break;
}

static int calc_reductions(const Node& node, const Move& m, Order& order, bool dangerous)
{
    assert(node.legals > 0);

    if (   node.depth >= 3
        && node.legals >= 2
        && !dangerous)
    {
        int red = LMR[min(node.depth, 63)][min(node.legals - 1, 63)];
        
        if (node.pv_node) red -= red / 2;

        int score = order.score();

        red -= score >=  6 * 1024;
        red += score <= -4 * 1024;

        red += node.entry.move.is_tactical();

        return max(red, 0);
    }

    else if (  !node.pv_node
            && node.depth >= 3
            && node.legals >= 3
            && dangerous
            && !node.pos.move_is_safe(m, order.see(node.pos, false)))
        return 1;

    return 0;
}

static int calc_extensions(const Node& node, const Move& m, Order& order, bool checks)
{
    if (order.singular())
        return 1;

    if (node.pv_node || node.depth <= 2) {
        if (checks)
            return 1;

        if (m.is_queen_promo())
            return 1;

        if (node.pos.move_is_recap(m))
            return 1;
    }

    return 0;
}

int search(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv, Move skip_move)
{
    assert(kstack.back() == pos.key());
    assert(alfa >= -ScoreMate && alfa < beta && beta <= ScoreMate);
    assert(depth <= DepthMax);

    if (depth <= 0) {
        assert(depth == 0);

        return qsearch(pos, alfa, beta, ply, 0, pv);
    }

    ttable.prefetch(pos.key());
    etable.prefetch(pos.key());

    si.depth_sel = max(si.depth_sel, ply + 1);

    // Do we need to abort the search?
    si.checkup();
  
    Node node(pos, alfa, beta, ply, depth, skip_move);

    pv[0] = MoveNone;

    int score;

    if (!node.root_node) {
        if (pos.draw_mat_insuf() || draw_rep(pos) || draw_fifty(pos))
            return ScoreDraw + 1 - (si.tnodes & 2);

        if (ply >= PliesMax)
            return pos.checkers() ? ScoreDraw : eval(pos, ply);

        // mate distance pruning
        
        alfa = max(mated_in(ply), alfa);
        beta = min(mate_in(ply + 1), beta);

        if (alfa >= beta) return alfa;
    }
    
    if ((node.hit = ttable.get(node.entry, node.key, node.ply))) {
        if (node.entry.depth >= depth && !node.pv_node) {
            bool cut =   node.entry.bound == BoundExact
                    || ((node.entry.bound & BoundLower) && node.entry.score >= beta)
                    || ((node.entry.bound & BoundUpper) && node.entry.score <= alfa);

            if (cut) return node.entry.score;
        }

        if (   SingularExt
            && !node.root_node
            && depth >= SEDepthMin
            && !node.skip_move
            && !pos.checkers()
            && !score_is_mate(node.entry.score)
            && (node.entry.bound & BoundLower)
            && node.entry.depth >= depth - SEDepthOffset)
            node.xtnd_move = node.entry.move;

        node.eval = node.entry.eval;
    }
    else
        node.eval = pos.checkers() ? mated_in(ply) : eval(pos, ply);

    Evals[ply] = node.eval;

    node.improving = ply < 2 || Evals[ply] > Evals[ply - 2];

    if (node.root_node && si.best_move)
        node.entry.move = si.best_move;

    if (node.hit) {
        bool adjust =   node.entry.bound == BoundExact
                   || ((node.entry.bound & BoundLower) && node.eval < node.entry.score)
                   || ((node.entry.bound & BoundUpper) && node.eval > node.entry.score);

        if (adjust) node.eval = node.entry.score;
    }

    history.clear_killers(pos.side(), ply + 2);

    if (pos.checkers()) goto move_loop;
   
    if (!node.pv_node && !node.skip_move) {

        if (   StaticNMP
            && depth <= StaticNMPDepthMax
            && pos.pieces(pos.side())
            && node.eval >= beta + (depth - node.improving) * StaticNMPFactor)
            return node.eval;

        if (   Razoring
            && depth <= 3
            && node.eval + RazoringFactor * depth < beta)
        {
            score = qsearch(pos, alfa, beta, ply, 0, pv);

            if (score < beta)
                return score;
            else if (depth == 1)
                return beta;
        }

        if (   NMPruning
            && depth >= NMPruningDepthMin
            && mstack.back(0) != MoveNull
            && mstack.back(1) != MoveNull
            && !score_is_mate(beta)
            && node.eval >= beta
            && pos.pieces(pos.side()))
        {
            int R = 3 + depth / 3;

            R = min(depth, R);
       
            UndoNull undo;

            pos.make_null(undo);
            score = -search(pos, -beta, -beta + 1, ply + 1, depth - R, node.pv);
            pos.unmake_null(undo);

            if (score >= beta)
                return score_is_mate(score) ? beta : score;
        }
    }

    if (depth <= 4) {
        score = node.eval + depth * FutilityFactor;

        if (score <= alfa) {
            node.bs = score;
            node.futile = true;
        }
    }

move_loop:

    MoveList qmoves;

    Order order(node, history);
    UndoMove undo;
    u64 pins = gen_pins(pos);

    for (Move m = order.next(); m; m = order.next()) {
        assert(alfa < beta);

        if (!pos.move_is_legal(m, pins)) [[unlikely]]
            continue;
        
        ++node.legals;

        if (m == node.skip_move)
            continue;
        
        bool checks     = pos.move_is_check(m);
        bool quiet      = !checks && !m.is_tactical();
        bool dangerous  = pos.checkers() || !quiet;

        int& see = order.see(pos, false);

        if (node.futile && (quiet || !pos.move_is_safe(m, see)))
            continue;

        if (!score_is_mate(node.bs)) {
            if (depth <= 5 && !dangerous && node.legals >= (depth + node.improving) * 3)
                continue;

            if (depth <= 6 && !dangerous && !pos.move_is_safe(m, see))
                continue;
           
            if (depth <= 4 && !quiet && !pos.move_is_safe(m, see))
                continue;
        }

        int ext = calc_extensions(node, m, order, checks);
        int red = calc_reductions(node, m, order, dangerous);

        assert(!ext || !red);

        if (ext == 0 && m == node.xtnd_move) {
            int lbound = node.entry.score - 2 * depth;

            score = search(pos, lbound - 1, lbound, ply, (depth - 1) / 2, node.pv, m);

            if (score < lbound)
                ext = 1;
            else if (lbound >= beta)
                return lbound;
        }

        int ndepth = depth + ext - 1;

        // Don't reduce to qs
        if (red && ndepth - red <= 0)
            red = ndepth - 1;

        // UCI info
        if (node.root_node) [[unlikely]] {
            si.curr_move     = m;
            si.curr_move_num = node.legals;
        }
       
        if (!m.is_tactical())
            qmoves.add(m);

        pos.make_move(m, undo);

        if ((node.pv_node && node.legals > 1) || red) {
            score = -search(pos, -alfa - 1, -alfa, ply + 1, ndepth - red, node.pv);

            if (score > alfa) {
                if (node.root_node) si.fail_highs++;
                score = -search(pos, -beta, -alfa, ply + 1, ndepth, node.pv);
                if (node.root_node) si.fail_highs--;
            }
        }
        else
            score = -search(pos, -beta, -alfa, ply + 1, ndepth, node.pv);

        pos.unmake_move(m, undo);

        if (score > node.bs) {
            node.bs = score;

            if (node.root_node && node.legals == 1)
                si.fail_low = node.bs <= alfa;

            if (node.bs > alfa) {
                alfa = node.bs;
                node.best_move = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, node.pv); 

                    if (node.root_node && node.legals > 1 && depth > 1)
                        si.update(depth, node.bs, pv, false);
                }

                if (alfa >= beta) {
                    if (!node.skip_move && !node.best_move.is_tactical())
                        history.update(node, qmoves);

                    break;
                }
            }
        }
    }

    if (!node.legals)
        return pos.checkers() ? mated_in(ply) : ScoreDraw;
    
    if (node.bs <= -ScoreMate) {
        assert(node.bs == -ScoreMate);
        return max(alfa, mated_in(ply + 1));
    }

    Entry& e = node.entry;

    e.move  = node.best_move;
    e.eval  = pos.checkers() ? -ScoreMate : Evals[ply];
    e.score = score_to_tt(node.bs, ply);
    e.depth = depth;
    e.bound = calc_bound(node.bs, node.orig_alfa, beta);

    ttable.set(e, node.key);

    return node.bs;
}

int qsearch(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv)
{
    assert(kstack.back() == pos.key());
    assert(depth <= 0);
    assert(depth <  0 || !pos.checkers());
    assert(alfa >= -ScoreMate && alfa < beta && beta <= ScoreMate);

    ttable.prefetch(pos.key());
    etable.prefetch(pos.key());

    // Do we need to abort the search?
    si.checkup();
   
    Node node(pos, alfa, beta, ply, depth);

    pv[0] = MoveNone;

    if (!node.root_node) {
        if (pos.draw_mat_insuf() || draw_rep(pos) || draw_fifty(pos))
            return ScoreDraw;

        if (ply >= PliesMax)
            return pos.checkers() ? ScoreDraw : eval(pos, ply);
    }

    node.hit = ttable.get(node.entry, node.key, node.ply);

    if (node.hit && node.entry.depth >= depth && !node.pv_node) {
        bool cut =   node.entry.bound == BoundExact
                || ((node.entry.bound & BoundLower) && node.entry.score >= beta)
                || ((node.entry.bound & BoundUpper) && node.entry.score <= alfa);

        if (cut) return node.entry.score;
    }

    if (pos.checkers())
        node.eval = node.bs = mated_in(ply);
    else {
        if (node.hit) {
            node.eval = node.bs = node.entry.eval;

            bool adjust =   node.entry.bound == BoundExact
                       || ((node.entry.bound & BoundLower) && node.eval < node.entry.score)
                       || ((node.entry.bound & BoundUpper) && node.eval > node.entry.score);

            if (adjust) node.bs = node.entry.score;
        }
        else
            node.eval = node.bs = eval(pos, ply);

        if (node.bs >= beta)
            return node.bs;

        if (node.bs > alfa)
            alfa = node.bs;
    }
    
    size_t qevasions = 0;

    Order order(node, history);
    UndoMove undo;
    u64 pins = gen_pins(pos);

    for (Move m = order.next(); m; m = order.next()) {
        assert(alfa < beta);
        
        if (!pos.move_is_legal(m, pins))
            continue;
        
        ++node.legals;

        if (node.bs > -ScoreMateMin) {

            if (depth == 0 && !m.is_tactical() && !pos.move_is_check(m))
                continue;

            if (qevasions >= 1) break;

            int see = order.see(pos, true);

            if (!see) {
                if (depth < 0) break;

                continue;
            }
        }

        pos.make_move(m, undo);
        int score = -qsearch(pos, -beta, -alfa, ply + 1, depth - 1, node.pv);
        pos.unmake_move(m, undo);
        
        qevasions += !m.is_tactical() && pos.checkers();

        if (score > node.bs) {
            node.bs = score;

            if (score > alfa) {
                alfa = score;
                node.best_move = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, node.pv);
                }
                
                if (alfa >= beta) break;
            }
        }
    }

    if (pos.checkers() && !node.legals)
        return mated_in(ply);

    if (node.bs <= -ScoreMate) {
        assert(node.bs == -ScoreMate);
        return max(alfa, mated_in(ply + 1));
    }

    if (depth == 0) {
        Entry& e = node.entry;

        e.move  = node.best_move;
        e.eval  = node.eval;
        e.score = score_to_tt(node.bs, ply);
        e.depth = 0;
        e.bound = calc_bound(node.bs, node.orig_alfa, beta);

        ttable.set(e, node.key);
    }

    return node.bs;
}

static int search_aspirate(Position pos, int depth, PV& pv, int score)
{
    assert(depth > 0);
        
    si.depth_sel = 0;

    if (depth <= 5)
        return search(pos, -ScoreMate, ScoreMate, 0, depth, pv); 

    int delta = AspMargin;
    int alfa  = max(score - delta, -ScoreMate);
    int beta  = min(score + delta,  ScoreMate);

    while (true) {
        score = search(pos, alfa, beta, 0, depth, pv);

        if (score <= alfa) {
            beta = (alfa + beta) / 2;
            alfa = max(alfa - delta, -ScoreMate);
        } 
        else if (score >= beta) {
            alfa = (alfa + beta) / 2;
            beta = min(beta + delta, ScoreMate);
        } 
        else
            return score;

        delta += delta / 2;
    }
}

void search_init(Position pos)
{
    MoveList moves;
    
    gen_moves(moves, pos, GenMode::Legal);

    if (moves.size() == 1) {
        si.singular = true;
        return;
    }

    if (!EasyMove || !sl.time_managed())
        return;

    MoveExtList smoves;
    PV pv;

    for (Move& m : moves) {
        UndoMove undo;

        pos.make_move(m, undo);
        int score = -search(pos, -ScoreMate, ScoreMate, 0, 1, pv); 
        pos.unmake_move(m, undo);
        
        smoves.add(m, score, ScoreNone);
    }
    
    assert(pos.key() == si.pos.key());

    smoves.sort();

    MoveExt m0 = smoves[0];
    MoveExt m1 = smoves[1];

    int diff = m0.score - m1.score;

    assert(diff >= 0);

    Entry e;

    if (diff >= EMMargin && ttable.get(e, pos.key(), 0) && e.move == m0.move)
        si.easy_move = m0.move;
}

void search_iterate()
{
    search_init(si.pos);

    si.initialized = true;
    
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

            if (si.score_drop)
                continue;

            double share = EMShare / 100.0;

            if (si.time_usage(sl) > share && si.best_move == si.easy_move && si.bm_updates == 0)
                break;
            
            if (si.time_usage(sl) >= 0.70)
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
   
    Razoring            = opt_list.get("Razoring").check_value();
    RazoringFactor      = opt_list.get("RazoringFactor").spin_value();
    
    AspMargin           = opt_list.get("AspMargin").spin_value();
    
    FutilityFactor      = opt_list.get("FutilityFactor").spin_value();

    EasyMove            = opt_list.get("EasyMove").check_value();
    EMMargin            = opt_list.get("EMMargin").spin_value();
    EMShare             = opt_list.get("EMShare").spin_value();
    
    GenState state = gen_state(si.pos);

    // No reason to search if there are no legal moves
    if (state != GenState::Normal) {
        gstats.num += !gstats.exc;
        gstats.stalemate += state == GenState::Stalemate;
        gstats.checkmate += state != GenState::Stalemate;

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

        int dn = si.depth_max;
        assert(dn > 0);

        gstats.depth_min  = min(gstats.depth_min, dn);
        gstats.depth_max  = max(gstats.depth_max, dn);
        gstats.depth_sum += dn;
        
        int ds = si.depth_sel;
        assert(ds > 0);

        gstats.seldepth_min  = min(gstats.seldepth_min, ds);
        gstats.seldepth_max  = max(gstats.seldepth_max, ds);
        gstats.seldepth_sum += ds;

        gstats.bm_updates += si.bm_updates;
        gstats.bm_stable += si.bm_stable;
   
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

bool draw_fifty(const Position& pos)
{
    if (pos.half_moves() < 100)
        return false;

    if (!pos.checkers())
        return true;

    MoveList moves;

    gen_moves(moves, pos, GenMode::Legal);

    return !moves.empty();
}

bool draw_rep(const Position& pos)
{
    for (size_t i = 4; i <= size_t(pos.half_moves()) && i < kstack.size(); i += 2)
        if (kstack.back(i) == kstack.back())
            return true;

    return false;
}

void search_reset()
{
    memset(Evals, 0, sizeof(Evals));

    history.clear();

    ttable.reset();
    etable.reset();
}

int mate_in(int ply)
{
    return ScoreMate - ply;
}

int mated_in(int ply)
{
    return ply - ScoreMate;
}

bool score_is_mate(int score)
{
    assert(abs(score) <= ScoreMate);

    return abs(score) >= ScoreMate - PliesMax;
}

void SearchInfo::update(int depth, int score, PV& pv, bool complete)
{
    assert(depth >= 1);
    assert(abs(score) < ScoreMate);
    assert(pv[0].is_valid());

    if (!initialized) return;

    i64 dur = timer.elapsed_time(1);

    // Don't flood stdout if time limited
    bool report = (depth > depth_max || pv[0] != best_move)
               && (!sl.time_limited() || depth >= 6 || dur >= 100);

    if (report) {
        ostringstream oss;

        assert(pv_is_valid(pos, pv));

        oss << "info" 
            << " depth "    << depth
            << " seldepth " << depth_sel
            << " score "    << uci_score(score)
            << " time "     << dur
            << " nodes "    << tnodes
            << " nps "      << 1000 * tnodes / dur
            << " hashfull " << ttable.permille()
            << " pv "       << pv_string(pv);

        uci_send(oss.str().c_str());
    }

    depth_max = depth;

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
        << " depth "    << depth_max
        << " seldepth " << depth_sel
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
        << " depth "            << depth_max
        << " seldepth "         << depth_sel
        << " time "             << dur
        << " nodes "            << tnodes
        << " nps "              << 1000 * tnodes / dur
        << " hashfull "         << ttable.permille()
        << " currmove "         << curr_move.str()
        << " currmovenumber "   << curr_move_num;

#if 0
    oss << endl << "info string ethitrate " << etable.hitrate();
#endif

    uci_send(oss.str().c_str());
}

// Depth limit is handled in search_iterate
void SearchInfo::checkup()
{
    if (!si.initialized) [[unlikely]]
        return;

    if (--cnodes >= 0) [[likely]]
        return;

    if (!best_move.is_valid()) [[unlikely]]
        return;

    i64 cnodes_next = 250000;
    i64 dur = timer.elapsed_time();

    bool abort = StopRequest;
    
    if (!abort && sl.nodes) {
        i64 diff = tnodes - sl.nodes;
            
        cnodes_next = -diff / 2;

        if (diff >= 0)
            abort = true;
    }

    if (!abort && sl.time_limited()) {
        i64 rem_time = 10000; // 10 seconds
       
        if (sl.move_time) {
            rem_time = min(rem_time, sl.move_time - dur);

            if (dur >= sl.move_time)
                abort = true;
        }

        else if (sl.time_managed()) {
            i64 xtime = sl.max_time;
            i64 ptime = sl.panic_time;

            double usage = double(dur) / xtime;

            rem_time = (dur >= xtime ? ptime : xtime) - dur;

            // Panic time threshold
            if (dur >= ptime)
                abort = true;

            else if (!fail_low) {

                if (usage >= 2.0)
                    abort = true;

                else if (usage >= 1.5 && curr_move_num == 1)
                    abort = true;

                else if (!fail_highs && !score_drop) {

                    if (usage >= 1.0)
                        abort = true;

                    if (usage >= 0.6) {

                        if (bm_updates == 0)
                            abort = true;

                    }
#if 0
                    if (usage >= 0.8) { // TODO tweak

                        if (bm_stable >= 10) // TODO tweak
                            abort = true;
                    }
#endif
                }
            }
        }

        i64 n = clamp(rem_time, i64(1), i64(10000)) * 25;

        cnodes_next = min(cnodes_next, n);
    }
   
    if (abort) [[unlikely]] 
        throw 1;
        
    cnodes = cnodes_next;

    if (depth_max >= 6) {
        if (dur > 5000 && dur - time_rep >= 1000) {
            time_rep = dur;

            uci_info(dur);
        }
    }
}
