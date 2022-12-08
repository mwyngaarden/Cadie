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
#include "move.h"
#include "pawn.h"
#include "pos.h"
#include "sort.h"
#include "tt.h"

using namespace std;

atomic_bool Searching = false;
atomic_bool StopRequest = false;

SearchInfo      sinfo;
SearchLimits    slimits;
MoveStack       move_stack;
KeyStack        key_stack;
thread          search_thread;

int Reductions[32][32];
int Evals[PliesMax];
Move Killers[2][PliesMax][2];

constexpr int HistoryMax = DepthMax * DepthMax;
static_assert(HistoryMax < numeric_limits<i16>::max());
i16 History[2][128][128];


static int isearch  (Position& pos);
static int  search  (Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv, Move sm = MoveNone);
static int qsearch  (Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv);

static int search_aspirate(Position& pos, int d, PV& pv, int score);

static void history_adjust(const Node& node, Move m, int bonus);

static void pv_append(PV& dst, const PV& src)
{
    for (int i = 0; i < PliesMax - 1; i++)
        if ((dst[i + 1] = src[i]) == MoveNone)
            break;
}

int calc_extensions(const Node& node, const Move& m, bool gives_check)
{
    return (node.depth <= Z0 && gives_check) || (node.pv_node && (gives_check || node.pos.move_is_recap(m)));
}

bool move_is_dangerous(const Node& node, const Move& m, bool gives_check)
{
    return m.is_tactical() || node.in_check || gives_check;
}

bool move_is_safe(const Node& node, const Move& m, int& see)
{
    if (m.is_under()) return false;

    u8 mpiece = node.pos[m.orig()];

    assert(piece256_is_ok(mpiece));

    if (is_king(mpiece)) return true;

    if (m.is_capture()) {
        u8 opiece = m.capture_piece();
    
        assert(piece256_is_ok(opiece));

        int mptype = P256ToP6[mpiece];
        int optype = P256ToP6[opiece];

        assert(piece_is_ok(mptype));
        assert(piece_is_ok(optype));

        if (optype >= mptype) return true;
    }

    if (see == -10000) see = see_move(node.pos, m);

    return see >= 0;
}

int calc_reductions(const Node& node, const Move& m, bool gives_check, int move_count, int& see)
{
    int red = 0;

    if (node.depth >= Z1 && move_count >= Z2 && !move_is_dangerous(node, m, gives_check)) {

        red = Reductions[min(node.depth, 31)][min(move_count, 31)];

        // Only extend 2/3 is PV node
        if (node.pv_node)
            red -= red / Z3;
    }
    else if (  !node.pv_node
            && node.depth >= Z4
            && move_count >= Z5
            && move_is_dangerous(node, m, gives_check)
            && !move_is_safe(node, m, see))
        red = 1;

    if (m == Killers[node.side][node.ply][0] || m == Killers[node.side][node.ply][1])
        red = max(0, red - 1);

    return red;
}

static string pv_string(PV& pv)
{
    ostringstream oss;

    for (size_t i = 0; pv[i] != MoveNone; i++) {
        if (i > 0) oss << ' ';

        oss << pv[i].to_string();
    }

    return oss.str();
}

int calc_reduction(int depth, int move)
{
    assert(depth > 0 && move > 0);

    double p0 = log(min(depth, 31)) / log(2);
    double p1 = log(min(move, 31)) / log(2);

    return round(p0 * p1 * 0.25);
}

void search_init()
{
    for (int d = 1; d < 32; d++)
        for (int m = 1; m < 32; m++)
            Reductions[d][m] = calc_reduction(d, m);
}

int search(Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv, Move sm)
{
    assert(key_stack.back() == pos.key());
    assert(alpha >= -ScoreMate && alpha < beta && beta <= ScoreMate);
    assert(depth <= DepthMax);

    if (depth <= 0)
        return qsearch(pos, alpha, beta, ply, 0, pv);

    thtable.prefetch(pos.key());
    ehtable.prefetch(pos.key());

    sinfo.seldepth = max(ply, sinfo.seldepth);

    gstats.nodes_search++;

    // Do we need to abort the search?
    sinfo.check();

    pv[0] = MoveNone;
   
    // set node score to -scoremate for white, inverse for black
    Node node(pos, alpha, beta, ply, depth, sm);

    PV pv_child;
    int depth_next;

    if (!node.root) {
        if (pos.draw_mat_insuf() || key_stack_rep(pos))
            return ScoreDraw;

        if (ply >= PliesMax)
            return node.in_check ? 0 : eval(pos, alpha, beta);

        alpha = max(mated_in(ply), alpha);
        beta  = min(mate_in(ply + 1), beta);

        if (alpha >= beta) return alpha;
    }

    bool tt_hit = false;

    if (UseTransTable) {
        tt_hit = thtable.get(node.tt_key, ply, node.tt_move, node.tt_eval, node.tt_score, node.tt_depth, node.tt_bound);

        if (tt_hit) {
            // set eval here from tt_eval

            if (node.tt_depth >= depth && !node.pv_node) {
                bool cutoff = node.tt_bound == BoundExact
                          || (node.tt_bound == BoundLower && node.tt_score >= beta)
                          || (node.tt_bound == BoundUpper && node.tt_score <= alpha);

                if (cutoff)
                    return node.tt_score;
            }

            // if .... set singular move and score
        }
    }
   
    // TODO refactor
    if (node.in_check)
        node.eval = -ScoreMate;
    else if (node.tt_eval != ScoreNone)
        node.eval = node.tt_eval;
    else if (move_stack.back() == MoveNull)
        node.eval = -Evals[node.ply - 1] + 2 * TempoBonus;
    else
        node.eval = eval(pos, alpha, beta);

    Evals[ply] = node.eval;

    bool improving = !node.in_check && (ply < 2 || Evals[ply] > Evals[ply - 2]);

    if (node.root && sinfo.move_best != MoveNone)
        node.tt_move = sinfo.move_best;

    if (!node.in_check && !node.pv_node) {

        if (Razoring) {
            if (depth <= 3 && node.eval + RazoringMargin < beta) {
                int score = qsearch(pos, alpha, beta, ply, 0, pv_child);

                if (score < beta)
                    return score;
                else if (depth == 1)
                    return beta;
            }
        }

        if (StaticNMP) {
            if (   depth <= StaticNMPDepthMax
                && pos.non_pawn_mat(node.side) 
                && node.eval >= beta + (depth - improving) * StaticNMPFactor)
            return node.eval;
        }

        if (NMPruning) {
            if (   depth >= NMPruningDepthMin
                && move_stack.back(0) != MoveNull
                && move_stack.back(1) != MoveNull
                // TODO double check this
                // && node.eval is not mate
                && node.eval >= beta
                && pos.non_pawn_mat(node.side))
            {
                bool null_ok = !tt_hit || node.tt_score >= beta || node.tt_bound < BoundUpper;

                if (null_ok) {
                    depth_next = depth - (3 + depth / 3);
               
                    NullUndo undo;

                    pos.make_null(undo);
                    int score = -search(pos, -beta, -beta + 1, ply + 1, depth_next, pv_child);
                    pos.unmake_null(undo);

                    if (score >= beta)
                        return score_is_mate(score) ? beta : score;
                }
            }
        }
    }

    if (!node.in_check && node.depth <= FutilityDepthMax) {
        int score = node.eval + node.depth * FutilityFactor;

        if (score <= alpha) {
            node.score = score;
            node.futile = true;
        }
    }

    // FIXME if futile only add tacticals and checks

    MoveList qmoves;
    
    Sort sort(node);

    int move_count = 0;
    MoveUndo undo;

    int sort_score;
    
    for (Move m = sort.next(sort_score); m; m = sort.next(sort_score)) {
        assert(alpha < beta);

        if (m == node.sm || !pos.move_is_legal(m)) continue;

        move_count++;

        if (node.sm && move_count >= SEMovesMax) break;
        
        bool gives_check = pos.move_is_check(m);

        int see = -10000;

        // TODO test with removal of m != node.tt_move
        if (   node.futile
            && m != node.tt_move
            && !gives_check 
            && !m.is_tactical())
            continue;

        // TODO test
        if (node.futile && !move_is_safe(node, m, see))
            continue;

        if (!score_is_mate(node.score)) {
            if (   node.depth <= Z7
                && move_count >= node.depth * Z8
                && !move_is_dangerous(node, m, gives_check))
                continue;

            if (   node.depth <= Z9
                && !move_is_dangerous(node, m, gives_check)
                && !move_is_safe(node, m, see))
                continue;
            
            if (   node.depth <= Z10
                && m.is_tactical()
                && !move_is_safe(node, m, see))
                continue;
        }

        int ext = calc_extensions(node, m, gives_check);
        int red = calc_reductions(node, m, gives_check, move_count - 1, see);

        if (!m.is_tactical()) qmoves.add(m);

        if (   SingularExt
            && !ext
            && m == node.tt_move
            && !node.root 
            && depth >= SEDepthMin
            && !node.sm
            && !node.in_check
            && !score_is_mate(node.tt_score)
            && node.tt_bound <= BoundExact
            && node.tt_depth >= depth - SEDepthOffset)
        {
            // TODO tune
            int lbound = node.tt_score - 4 * depth;

            int score = search(pos, lbound, lbound + 1, ply, depth - 4, pv_child, m);

            ext = score <= lbound;
        }

        pos.make_move(m, undo);

        depth_next = depth + ext - 1;
       
        if (red && depth_next - red <= 0) red = depth_next - 1;

        if ((node.pv_node && move_count > 1) || red) {
            node.score = -search(pos, -alpha - 1, -alpha, ply + 1, depth_next - red, pv_child);

            if (node.score > alpha)
                node.score = -search(pos, -beta, -alpha, ply + 1, depth_next, pv_child);
        }
        else
            node.score = -search(pos, -beta, -alpha, ply + 1, depth_next, pv_child);
        
        pos.unmake_move(m, undo);

        if (node.score > node.bs) {
            node.bs = node.score;

            if (node.score > alpha) {
                alpha = node.score;
                node.bm = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, pv_child); 

                    if (node.root && move_count > 1 && depth > 1)
                        sinfo.update(depth, node.score, pv, false);
                }

                // fail high
                if (alpha >= beta) break;
            }
        }
    }

    if (!move_count) {
        if (node.sm)
            return alpha;
        else if (node.in_check)
            return mated_in(ply);
        else
            return ScoreDraw;
    }

    if (node.bs <= -ScoreMate)
        return max(alpha, mated_in(ply + 1));

    if (!node.sm && alpha > node.alpha && !node.bm.is_tactical()) {
        assert(node.bm != MoveNone);

        if (KillerMoves) {
            if ((node.bm != Killers[node.side][node.ply][0] && node.bm != Killers[node.side][node.ply][1])
                || node.bm == Killers[node.side][node.ply][1]) {
                Killers[node.side][node.ply][1] = Killers[node.side][node.ply][0];
                Killers[node.side][node.ply][0] = node.bm;
            }
        }

        history_adjust(node, node.bm, depth * depth);

        for (Move& m : qmoves) {
            if (m != node.bm) {
                int bonus = -1 - depth * depth / 2;

                history_adjust(node, m, bonus);
            }
        }
    }

    if (UseTransTable) {
        node.tt_bound = node.bs >= beta ? BoundLower : node.bs > node.alpha ? BoundExact : BoundUpper;

        thtable.set(node.tt_key, ply, node.bm, node.eval, node.bs, depth, node.tt_bound);
    }

    return node.bs;
}

int qsearch(Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv)
{
    assert(key_stack.back() == pos.key());
    assert(depth <= 0);
    assert(alpha >= -ScoreMate && alpha < beta && beta <= ScoreMate);
     
    thtable.prefetch(pos.key());
    ehtable.prefetch(pos.key());
    
    sinfo.seldepth = max(ply, sinfo.seldepth);

    gstats.nodes_qsearch++;

    // Do we need to abort the search?
    sinfo.check();
    
    Node node(pos, alpha, beta, ply, depth);

    PV pv_child;

    if (node.pv_node)
        pv[0] = MoveNone;

    if (!node.root) {
        if (pos.draw_mat_insuf() || key_stack_rep(pos))
            return ScoreDraw;

        if (ply >= PliesMax)
            return eval(pos, alpha, beta);
            // TODO check
            //return node.in_check ? 0 : eval(pos, alpha, beta);
    }

    if (UseTransTable) {
        bool tt_hit = thtable.get(pos.key(), ply, node.tt_move, node.tt_eval, node.tt_score, node.tt_depth, node.tt_bound);

        if (tt_hit) {
            bool cutoff = node.tt_bound == BoundExact
                || (node.tt_bound == BoundLower && node.tt_score >= beta)
                || (node.tt_bound == BoundUpper && node.tt_score <= alpha);

            if (cutoff)
                return node.tt_score;
        }
    }

    // TODO refactor
    if (node.in_check)
        node.eval = -ScoreMate;
    else if (node.tt_eval != ScoreNone)
        node.eval = node.tt_eval;
    else if (move_stack.back() == MoveNull)
        node.eval = -Evals[node.ply - 1] + 2 * TempoBonus;
    else
        node.eval = eval(pos, alpha, beta);

    Evals[ply] = node.eval;

    if (!node.in_check) {
        node.bs = node.eval;

        if (node.bs > alpha) {
            alpha = node.bs;

            if (node.bs >= beta)
                return node.bs;
        }
    }
    
    Sort sort(node);

    int move_count = 0;
    MoveUndo undo;

    int sort_score;

    for (Move m = sort.next(sort_score); m; m = sort.next(sort_score)) {
        assert(alpha < beta);

        int see = -10000;

        if (!node.in_check && depth <= DepthMin && !pos.move_is_recap(m))
            continue;

        if (   DeltaPruning 
            && !node.in_check 
            && (node.eval + pos.see_max(m) + DPMargin) <= alpha
            && !(depth == 0 && pos.move_is_check(m)))
            continue;

        if (!node.in_check && !move_is_safe(node, m, see))
            continue;

        if (!pos.move_is_legal(m)) continue;

        pos.make_move(m, undo);
        move_count++;
        node.score = -qsearch(pos, -beta, -alpha, ply + 1, depth - 1, pv_child);
        pos.unmake_move(m, undo);

        if (node.score > node.bs) {
            node.bs = node.score;

            if (node.score > alpha) {
                alpha = node.score;
                node.bm = m;

                if (node.pv_node) {
                    pv[0] = m;

                    pv_append(pv, pv_child);
                }
            
                // fail high
                if (alpha >= beta) break;
            }
        }
    }

    if (node.in_check && !move_count)
        return mated_in(ply);

    if (node.bs <= -ScoreMate) {
        assert(node.bs == -ScoreMate);

        return max(alpha, mated_in(ply + 1));
    }

    if (UseTransTable) {
        node.tt_bound = node.bs >= beta ? BoundLower : node.bs > node.alpha ? BoundExact : BoundUpper;

        thtable.set(pos.key(), ply, node.bm, node.eval, node.bs, 0, node.tt_bound);
    }

    return node.bs;
}

static int search_aspirate(Position& pos, int depth, PV& pv, int score)
{
    assert(depth > 0);

    if (depth == 1)
        return search(pos, -ScoreMate, ScoreMate, 0, depth, pv); 

    int delta   = AspMargin;
    int alpha   = max(score - delta, -ScoreMate);
    int beta    = min(score + delta,  ScoreMate);
    
    while (true) {
        score = search(pos, alpha, beta, 0, depth, pv);

        if (score <= alpha) {
            beta  = (alpha + beta) / 2;
            alpha = max(alpha - delta, -ScoreMate);
        } 
        else if (score >= beta) {
            alpha = (alpha + beta) / 2;
            beta  = min(beta + delta, ScoreMate);
        } 
        else
            return score;

        delta += delta / 2;
    }
}

int isearch(Position& pos)
{
    int depth = slimits.depth ? slimits.depth : DepthMax;
    int score = 0;
    
    PV pv;

    for (int d = 1; d <= depth; d++) {
        score = search_aspirate(pos, d, pv, score);

        assert(pos == sinfo.pos);

        sinfo.update(d, score, pv, true);

        if (slimits.using_tm() && slimits.time_elapsed() > 0.46)
            break;
    }

    return score;
}

void search_go()
{
    if (setjmp(sinfo.jump_buffer) != 0)
        return;

    thtable.inc_gen();

    Position pos = sinfo.pos;

    isearch(pos);
}

void search_start()
{
    Searching = true;

    UseTransTable       = opt_list.get("UseTransTable").check_value();
    KillerMoves         = opt_list.get("KillerMoves").check_value();

    NMPruning           = opt_list.get("NMPruning").check_value();
    NMPruningDepthMin   = opt_list.get("NMPruningDepthMin").spin_value();
    
    SingularExt         = opt_list.get("SingularExt").check_value();
    SEMovesMax          = opt_list.get("SEMovesMax").spin_value();
    SEDepthMin          = opt_list.get("SEDepthMin").spin_value();
    SEDepthOffset       = opt_list.get("SEDepthOffset").spin_value();

    StaticNMP           = opt_list.get("StaticNMP").check_value();
    StaticNMPDepthMax   = opt_list.get("StaticNMPDepthMax").spin_value();
    StaticNMPFactor     = opt_list.get("StaticNMPFactor").spin_value();
   
    DeltaPruning        = opt_list.get("DeltaPruning").check_value();
    DPMargin            = opt_list.get("DPMargin").spin_value();
    
    Razoring            = opt_list.get("Razoring").check_value();
    RazoringMargin      = opt_list.get("RazoringMargin").spin_value();
    
    QSearchMargin       = opt_list.get("QSearchMargin").spin_value();
    AspMargin           = opt_list.get("AspMargin").spin_value();
    TempoBonus          = opt_list.get("TempoBonus").spin_value();
    
    UseLazyEval         = opt_list.get("UseLazyEval").check_value();
    LazyMargin          = opt_list.get("LazyMargin").spin_value();
    
    UciNodesBuffer      = opt_list.get("UciNodesBuffer").spin_value();
    
    FutilityFactor      = opt_list.get("FutilityFactor").spin_value();
    FutilityDepthMax    = opt_list.get("FutilityDepthMax").spin_value();
    
    Z0                  = opt_list.get("Z0").spin_value();
    Z1                  = opt_list.get("Z1").spin_value();
    Z2                  = opt_list.get("Z2").spin_value();
    Z3                  = opt_list.get("Z3").spin_value();
    Z4                  = opt_list.get("Z4").spin_value();
    Z5                  = opt_list.get("Z5").spin_value();
    Z7                  = opt_list.get("Z7").spin_value();
    Z8                  = opt_list.get("Z8").spin_value();
    Z9                  = opt_list.get("Z9").spin_value();
    Z10                 = opt_list.get("Z10").spin_value();

    GenState gstate = gen_state(sinfo.pos);

    if (gstate != GenState::Normal) {
        int score;
            
        gstats.num += !gstats.exc;

        if (gstate == GenState::Stalemate) {
            score = ScoreDraw;
            gstats.stalemate++;
        }
        else {
            score = sinfo.pos.side() == White ? -ScoreMate : ScoreMate;
            gstats.checkmate++;
        }
        
        uci_send("info depth 0 score %s", uci_score(score).c_str());
        uci_send("bestmove (none)");
    }

    else {
        gstats.stimer.start(gstats.benchmarking);
        search_go();
        gstats.stimer.stop(gstats.benchmarking);
        gstats.stimer.accrue(gstats.benchmarking);

        sinfo.uci_bestmove();

        gstats.unfinished++;
        gstats.num++;
        
        int d = sinfo.depth_max;

        assert(d >= 0);

        gstats.depth_min  = min(gstats.depth_min, d);
        gstats.depth_max  = max(gstats.depth_max, d);
        gstats.depth_sum += d;
   
        i64 n = sinfo.nodes;

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

bool key_stack_rep(const Position& pos)
{
    if (pos.half_moves() >= 100)
        return !pos.in_check() || gen_state(pos) == GenState::Normal;

    for (size_t i = 4; i <= (size_t)pos.half_moves() && i < key_stack.size(); i += 2)
        if (key_stack.back(i) == key_stack.back())
            return true;

    return false;
}

void search_reset()
{
    memset(History, 0, sizeof(History));
    memset(Evals,   0, sizeof(Evals));
    memset(Killers, 0, sizeof(Killers));

    ehtable.reset();
    phtable.reset();
    thtable.reset();
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

void history_adjust(const Node& node, Move m, int bonus)
{
    int orig = m.orig();
    int dest = m.dest();

    int value = History[node.side][orig][dest];

    value += 32 * bonus - value * abs(bonus) / 128;
    value  = clamp(value, -HistoryMax, HistoryMax);

    History[node.side][orig][dest] = value;
}

void SearchInfo::update(int depth, int score, PV& pv, bool complete)
{
    if (depth <= depth_max)
        return;

    i64 dur = slimits.timer.elapsed_time(1);

    if (depth >= slimits.depth_min() - 1) {
        ostringstream oss;

        oss << "info" 
            << " depth " << depth
            << " seldepth " << sinfo.seldepth
            << " score " << uci_score(score)
            << " time " << dur
            << " nodes " << nodes
            << " nps " << 1000 * nodes / dur
            << " hashfull " << thtable.permille()
            << " pv " << pv_string(pv);

        uci_send(oss.str().c_str());
    }

    if (complete)
        depth_max = depth;

    assert(pv[0] != MoveNone);

    stability = pv[0] == move_best ? stability + 1 : 0;

    score_best = score;
    move_best = pv[0];
}

void SearchInfo::uci_bestmove() const
{
    std::ostringstream oss;

    oss << "bestmove " << sinfo.move_best.to_string();

    uci_send(oss.str().c_str());
}

void SearchInfo::check()
{
    ++nodes;

    if (--nodes_countdown >= 0)
        return;

    if (move_best == MoveNone)
        return;
    
    if (!slimits.nodes)
        nodes_countdown += UciNodesBuffer;
    
    if (slimits.using_tm() && depth_max < slimits.depth_min())
        return;

    bool abort = false;

    if (StopRequest)
        abort = true;
    
    else if (slimits.nodes) {
        i64 diff = nodes - slimits.nodes;

        if (diff >= 0)
            abort = true;
        else
            nodes_countdown = -diff / 10;
    }

    else if (slimits.depth && depth_max >= slimits.depth)
        abort = true;

    else {
        i64 dur = slimits.timer.elapsed_time();

        assert(dur >= 0);

        if (slimits.movetime && dur >= slimits.movetime)
            abort = true;

        else if (slimits.time) {
            i64 delta   = slimits.time_max - slimits.time_min;
            i64 margin  = delta * pow(0.9, stability);

            if (dur >= slimits.time_min + margin)
                abort = true;
        }
    }
   
    if (abort)
        longjmp(jump_buffer, 1);
}
