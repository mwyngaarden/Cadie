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
#include "math.h"
#include "move.h"
#include "pawn.h"
#include "pos.h"
#include "order.h"
#include "tt.h"

using namespace std;

atomic_bool Searching = false;
atomic_bool StopRequest = false;

SearchInfo      sinfo;
SearchLimits    slimits;
MoveStack       mstack;
KeyStack        kstack;

int LMReductions[64][64];
int Evals[PliesMax];

History history;

static int qsearch(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv);
static int  search(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv, Move sm = MoveNone);

static void search_init(Position pos);
static int search_aspirate(Position pos, int depth, PV& pv, int score);
static void search_iterate();

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
    MoveList moves;

    for (size_t i = 0; pv[i] != MoveNone; i++) {
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

    return log2(d) * log2(m) * 0.25 + 0.5;
}

void search_init()
{
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++)
            LMReductions[d][m] = calc_lmr(d, m);
}

static void pv_append(PV& dst, const PV& src)
{
    for (int i = 0; i < PliesMax - 1; i++)
        if ((dst[i + 1] = src[i]) == MoveNone)
            break;
}

static int calc_reductions(const Node& node, const Move& m, bool gives_check, Order& order)
{
    assert(node.legals > 0);

    int score = order.score();

    int red = 0;

    if (   node.depth >= 3
        && node.legals >= 2
        && !node.pos.move_is_dangerous(m, gives_check))
    {
        int i = min(node.depth, 63);
        int j = min(node.legals - 1, 63);

        red = LMReductions[i][j];

        if (node.pv_node)
            red -= red / 2;

        if (Order::special(score))
            --red;
        else {
            red -= score >=  2048;
            red += score <= -1024;
        }

        red += node.entry.move.is_tactical();
    }

    else if (  !node.pv_node
            && node.depth >= 3
            && node.legals >= 4
            && node.pos.move_is_dangerous(m, gives_check)
            && !node.pos.move_is_safe(m, order.see(node.pos, false)))
        red = 1;

    return max(red, 0);
}

static int calc_extensions(const Node& node, const Move& m, bool gives_check, Order& order)
{
    if (node.depth <= 2) {
        if (gives_check)
            return 1;
    
        if (node.pos.move_is_recap(m))
            return 1;
    }

    if (order.singular())
        return 1;

    if (node.pv_node) {
        if (gives_check)
            return 1;
       
        if (!m.is_under() && m.is_promo())
            return 1;

        if (node.pos.move_is_recap(m))
            return 1;
    }

    return 0;
}

int search(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv, Move sm)
{
    assert(kstack.back() == pos.key());
    assert(alfa >= -ScoreMate && alfa < beta && beta <= ScoreMate);
    assert(depth <= DepthMax);

    if (depth <= 0)
        return qsearch(pos, alfa, beta, ply, 0, pv);

    ttable.prefetch(pos.key());
    etable.prefetch(pos.key());

    sinfo.depth_sel = max(sinfo.depth_sel, ply + 1);

    // Do we need to abort the search?
    sinfo.checkup();
  
    Node node(pos, alfa, beta, ply, depth, sm);

    pv[0] = MoveNone;

    int score;
    int ndepth;

    if (!node.root) {
        if (pos.draw_mat_insuf() || kstack_rep(pos))
            return ScoreDraw;

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

        node.eval = node.entry.eval;
    }
    else
        node.eval = pos.checkers() ? mated_in(ply) : eval(pos, ply);

    Evals[ply] = node.eval;

    node.improving = !pos.checkers() && ply >= 2 && Evals[ply] > Evals[ply - 2];

    if (node.root && sinfo.bm)
        node.entry.move = sinfo.bm;

    if (node.hit) {
        bool adjust =   node.entry.bound == BoundExact
                   || ((node.entry.bound & BoundLower) && node.eval < node.entry.score)
                   || ((node.entry.bound & BoundUpper) && node.eval > node.entry.score);

        if (adjust) node.eval = node.entry.score;
    }

    history.clear_killers(!pos.side(), ply + 1);

    if (pos.checkers()) goto move_loop;
   
    if (!node.pv_node && !node.sm) {

        if (   StaticNMP
            && depth <= StaticNMPDepthMax
            && pos.non_pawn_mat(pos.side())
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
            && pos.non_pawn_mat(pos.side()))
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
    MoveList cmoves;
    Order order(node, history);
    UndoMove undo;

    for (Move m = order.next(); m; m = order.next()) {
        assert(alfa < beta);

        if (!pos.move_is_legal(m))
            continue;
        
        ++node.legals;

        if (m == node.sm)
            continue;
        
        bool gives_check = pos.move_is_check(m);

        int& see = order.see(pos, false);

        if (node.futile) {
            if (!m.is_tactical() && !gives_check)
                continue;

            if (!pos.move_is_safe(m, see))
                continue;
        }

        if (!score_is_mate(node.bs)) {
            if (   depth <= 4
                && node.legals >= depth * 4
                && !pos.move_is_dangerous(m, gives_check))
                continue;

            if (   depth <= 6
                && !pos.move_is_dangerous(m, gives_check)
                && !pos.move_is_safe(m, see))
                continue;
            
            if (   depth <= 4
                && m.is_tactical()
                && !pos.move_is_safe(m, see))
                continue;
        }

        int ext = calc_extensions(node, m, gives_check, order);
        int red = calc_reductions(node, m, gives_check, order);

        assert(!ext || !red);

        if (   SingularExt
            && !ext
            && m == node.entry.move
            && !node.root 
            && depth >= SEDepthMin
            && !node.sm
            && !pos.checkers()
            && !score_is_mate(node.entry.score)
            && (node.entry.bound & BoundLower)
            && node.entry.depth >= depth - SEDepthOffset)
        {
            int lbound = node.entry.score - 2 * depth;

            score = search(pos, lbound - 1, lbound, ply, depth / 2, node.pv, m);

            if (score < lbound)
                ext = 1;
            else if (lbound >= beta)
                return lbound;
        }

        ndepth = depth + ext - 1;
        assert(ndepth >= 0);

        // Don't reduce to qs
        if (red && ndepth - red <= 0)
            red = ndepth - 1;

        if (node.root) {
            sinfo.cm     = m;
            sinfo.cm_num = node.legals;
        }
        
        if (!m.is_tactical())
            qmoves.add(m);
        else if (m.is_capture())
            cmoves.add(m);

        pos.make_move(m, undo);

        if ((node.pv_node && node.legals > 1) || red) {
            score = -search(pos, -alfa - 1, -alfa, ply + 1, ndepth - red, node.pv);

            if (score > alfa) {
                if (node.root) sinfo.fail_highs++;
                score = -search(pos, -beta, -alfa, ply + 1, ndepth, node.pv);
                if (node.root) sinfo.fail_highs--;
            }
        }
        else
            score = -search(pos, -beta, -alfa, ply + 1, ndepth, node.pv);

        pos.unmake_move(m, undo);

        if (score > node.bs) {
            node.bs = score;

            if (node.root && node.legals == 1)
                sinfo.fail_low = node.bs <= alfa;

            if (node.bs > alfa) {
                alfa = node.bs;
                node.bm = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, node.pv); 

                    if (node.root && node.legals > 1 && depth > 1)
                        sinfo.update(depth, node.bs, pv, false);
                }

                if (alfa >= beta) {
                    history.update(node, qmoves, cmoves);
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

    e.move  = node.bm;
    e.eval  = pos.checkers() ? -ScoreMate : Evals[ply];
    e.score = score_to_tt(node.bs, ply);
    e.depth = depth;
    e.bound = calc_bound(node.bs, node.oalfa, beta);

    ttable.set(e, node.key);

    return node.bs;
}

int qsearch(Position& pos, int alfa, int beta, const int ply, const int depth, PV& pv)
{
    assert(kstack.back() == pos.key());
    assert(depth <= 0);
    assert(alfa >= -ScoreMate && alfa < beta && beta <= ScoreMate);

    ttable.prefetch(pos.key());
    etable.prefetch(pos.key());

    // Do we need to abort the search?
    sinfo.checkup();
   
    Node node(pos, alfa, beta, ply, depth);

    pv[0] = MoveNone;

    if (!node.root) {
        if (pos.draw_mat_insuf() || kstack_rep(pos))
            return ScoreDraw;

        if (ply >= PliesMax)
            return pos.checkers() ? 0 : eval(pos, ply);
    }

    i8 tt_depth = depth < 0 && !node.pos.checkers() ? -1 : 0;

    node.hit = ttable.get(node.entry, node.key, node.ply);

    if (node.hit && node.entry.depth >= tt_depth && !node.pv_node) {
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

    Order order(node, history);
    UndoMove undo;

    size_t qevasions = 0;

    for (Move m = order.next(); m; m = order.next()) {
        assert(alfa < beta);
        
        if (!pos.move_is_legal(m))
            continue;
        
        ++node.legals;

        if (node.bs > -ScoreMateMin) {

            if (!pos.checkers()) {

                if (depth <= DepthMin && !pos.move_is_recap(m))
                    continue;
           
                if (depth == 0 && !m.is_tactical() && !pos.move_is_check(m))
                    continue;

                if (!pos.move_is_safe(m, order.see(pos, false)))
                    continue;
            }

            if (qevasions >= 1)
                break;
        }

        qevasions += !m.is_tactical() && pos.checkers();

        pos.make_move(m, undo);
        int score = -qsearch(pos, -beta, -alfa, ply + 1, depth - 1, node.pv);
        pos.unmake_move(m, undo);

        if (score > node.bs) {
            node.bs = score;

            if (score > alfa) {
                alfa = score;
                node.bm = m;

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

    Entry& e = node.entry;

    e.move  = node.bm;
    e.eval  = node.eval;
    e.score = score_to_tt(node.bs, ply);
    e.depth = tt_depth;
    e.bound = calc_bound(node.bs, node.oalfa, beta);

    ttable.set(e, node.key);

    return node.bs;
}

static int search_aspirate(Position pos, int depth, PV& pv, int score)
{
    assert(depth > 0);
        
    sinfo.depth_sel = 0;

    if (depth == 1)
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

        delta += max(delta / 2, 1);
    }
}

void search_init(Position pos)
{
    MoveList moves;
    
    gen_moves(moves, pos, GenMode::Legal);

    if (moves.size() == 1) {
        sinfo.singular = true;
        return;
    }

    if (!EasyMove)
        return;

    Entry e;
    MoveExtList smoves;
    PV pv;

    for (Move& m : moves) {
        int score;

        UndoMove undo;

        pos.make_move(m, undo);
        score = -search(pos, -ScoreMate, ScoreMate, 0, 1, pv); 
        pos.unmake_move(m, undo);
        
        smoves.add(m, score, ScoreNone);
    }
    
    assert(pos.key() == sinfo.pos.key());

    smoves.sort();

    MoveExt m0 = smoves[0];
    MoveExt m1 = smoves[1];

    int diff = m0.score - m1.score;

    assert(diff >= 0);

    if (diff >= EMMargin && ttable.get(e, pos.key(), 0) && e.move == m0.move)
        sinfo.em = m0.move;
}

void search_iterate()
{
    search_init(sinfo.pos);

    sinfo.initialized = true;
    
    PV pv;

    int score = 0;

    for (int depth = 1; depth <= DepthMax; depth++) {

        try {
            score = search_aspirate(sinfo.pos, depth, pv, score);
        } catch (int i) {
            break;
        }

        // Aspiration completed successfully without reaching time limit or node limit

        sinfo.update(depth, score, pv);

        if (slimits.depth && depth >= slimits.depth)
            break;

        if (slimits.time) {
            if (sinfo.singular)
                break;

            if (sinfo.score_drop)
                continue;

            double share = EMShare / 100.0;

            if (sinfo.time_share_min(slimits) > share && sinfo.bm == sinfo.em && sinfo.bm_updates == 0)
                break;
            
            if (sinfo.time_share_max(slimits) >= 0.46)
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
    
    GenState state = gen_state(sinfo.pos);

    // No reason to search if there are no legal moves
    if (state != GenState::Normal) {
        gstats.num += !gstats.exc;
        gstats.stalemate += state == GenState::Stalemate;
        gstats.checkmate += state != GenState::Stalemate;

        int score = state == GenState::Stalemate
                  ? ScoreDraw
                  : (sinfo.pos.side() == White ? -ScoreMate : ScoreMate);
        
        uci_send("info depth 0 score %s", uci_score(score).c_str());
        uci_send("bestmove (none)");
    }

    else {
        gstats.stimer.start(true);

        search_iterate();

        sinfo.uci_bestmove();

        gstats.stimer.stop(true);
        gstats.stimer.accrue(true);
    
        // Collect statistics
        
        gstats.normal++;
        gstats.num++;

        int dn = sinfo.depth_max;
        assert(dn > 0);

        gstats.depth_min  = min(gstats.depth_min, dn);
        gstats.depth_max  = max(gstats.depth_max, dn);
        gstats.depth_sum += dn;
        
        int ds = sinfo.depth_sel;
        assert(ds > 0);

        gstats.seldepth_min  = min(gstats.seldepth_min, ds);
        gstats.seldepth_max  = max(gstats.seldepth_max, ds);
        gstats.seldepth_sum += ds;

        gstats.bm_updates += sinfo.bm_updates;
        gstats.bm_stable += sinfo.bm_stable;
   
        i64 n = sinfo.tnodes;

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

bool kstack_rep(const Position& pos)
{
    if (pos.half_moves() >= 100) {
        if (!pos.checkers())
            return true;

        MoveList moves;

        gen_moves(moves, pos, GenMode::Legal);

        return moves.size() > 0;
    }

    for (size_t i = 4; i <= (size_t)pos.half_moves() && i < kstack.size(); i += 2)
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
    ptable.reset();
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
    bool send = !slimits.time_limited() || depth >= 6 || dur >= 100;

    if (send) {
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

    if (pv[0] == bm)
        bm_stable   += complete;
    else {
        bm_stable    = 0;
        bm_updates  += depth > 1;
    }

    fail_low = false;

    score_drop = score - score_prev <= -10;
    score_prev = score;

    bm = pv[0];
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

    oss << "bestmove " << sinfo.bm.str();

    uci_send(oss.str().c_str());
}

void SearchInfo::uci_report(i64 dur) const
{
    ostringstream oss;

    oss << "info" 
        << " depth "            << depth_max
        << " seldepth "         << depth_sel
        << " time "             << dur
        << " nodes "            << tnodes
        << " nps "              << 1000 * tnodes / dur
        << " hashfull "         << ttable.permille()
        << " currmove "         << cm.str()
        << " currmovenumber "   << cm_num;

#if 0
    oss << endl 
        << "info string ethitrate " << etable.hitrate() 
        << " pthitrate " << ptable.hitrate();
#endif

    uci_send(oss.str().c_str());
}

double SearchInfo::factor() const
{
    const double N = 3;

    double s = 1 + bm_stable;
    double t = N + bm_stable + bm_updates;

    double cdf = math::binomial_cdf(s, t, (N - 1) / N);

    assert(cdf >= 0 && cdf <= 1);

    return 1 - cdf;
}

// Depth limit is handled in search_iterate
void SearchInfo::checkup()
{
    if (!sinfo.initialized) [[unlikely]]
        return;

    if (--cnodes >= 0)
        return;

    if (!bm.is_valid())
        return;

    i64 cnodes_next = 250000;
    i64 dur = timer.elapsed_time();

    bool abort = StopRequest;
    
    if (!abort && slimits.nodes) {
        i64 diff = tnodes - slimits.nodes;
            
        cnodes_next = -diff / 2;

        if (diff >= 0)
            abort = true;
    }

    if (!abort && slimits.time_limited()) {
        i64 rem = 10000; // 10 seconds
       
        if (slimits.movetime) {
            rem = min(rem, slimits.movetime - dur);

            if (dur >= slimits.movetime)
                abort = true;
        }

        else if (slimits.time) {
            i64 ntime = slimits.time_min;
            i64 xtime = slimits.time_max;
            i64 ptime = slimits.time_panic;

            // Minimum time threshold
            if (dur < ntime)
                rem = min(rem, ntime - dur);

            // Panic time threshold
            else if (dur >= ptime)
                abort = true;

            // Normal maximum time threshold
            else if (dur >= xtime) {
                rem = min(rem, ptime - dur);

                if (!fail_low)
                    abort = true;
            }

            // Normal time threshold
            else {
                rem = min(rem, xtime - dur);

                if (!fail_low) {
                    i64 delta = xtime - ntime;
                    i64 margin = delta * (is_stable() ? factor() : 1.0);
                    
                    if (dur >= ntime + margin)
                        abort = true;
                }
            }
        }

        i64 n = clamp(rem, (i64)1, (i64)10000) * 25;

        cnodes_next = min(cnodes_next, n);
    }
   
    if (abort) throw 1;
        
    cnodes = cnodes_next;

    if (depth_max >= 6) {
        if (dur > 5000 && dur - time_rep >= 1000) {
            time_rep = dur;

            uci_report(dur);
        }
    }
}
