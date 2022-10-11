#include <algorithm>
#include <array>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstring>
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

atomic_bool Stop = false;

SearchInfo      search_info;
SearchLimits    search_limits;
MoveStack       move_stack;
KeyStack        key_stack;
thread          search_thread;

int LMReduction[32][32];
int Evals[PliesMax];
Move Killers[2][PliesMax][2];


constexpr int HistoryMax = DepthMax * DepthMax;
static_assert(HistoryMax < numeric_limits<i16>::max());
i16 History[2][128][128];

static int  search(Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv, Move sm = MoveNone);
static int qsearch(Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv);

static int search_aspirate(Position& pos, int d, PV& pv, int score);

static void history_adjust(const Node& node, Move m, int bonus);

static void pv_append(PV& dst, PV& src)
{
    for (int i = 0; i < PliesMax - 1; i++)
        if ((dst[i + 1] = src[i]) == MoveNone)
            break;
}

int calc_extensions(const Node& node, const Move& m, bool gives_check)
{
    return (node.depth <= Z0 && gives_check)
        || (node.pv_node && (gives_check || node.pos.move_is_recap(m)));
}

bool move_is_dangerous(const Node& node, const Move& m, bool gives_check)
{
    return m.is_tactical() || node.in_check || gives_check;
}

bool move_is_safe(const Node& node, const Move& m, int& see)
{
    if (m.is_under()) return false;

    const u8 mpiece = node.pos[m.orig()];

    assert(piece256_is_ok(mpiece));

    if (is_king(mpiece)) return true;

    if (m.is_capture()) {
        const u8 opiece = m.capture_piece();
    
        assert(piece256_is_ok(opiece));

        const int mptype = P256ToP6[mpiece];
        const int optype = P256ToP6[opiece];

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

        red = LMReduction[min(node.depth, 31)][min(move_count, 31)];

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

int search_reduction(int depth, int move)
{
    assert(depth > 0 && move > 0);

    const double p0 = log(min(depth, 31)) / log(2);
    const double p1 = log(min(move, 31)) / log(2);

    // TODO tune
    return round(p0 * p1 * 0.25);
}

void search_init()
{
    for (int d = 1; d < 32; d++)
        for (int m = 1; m < 32; m++)
            LMReduction[d][m] = search_reduction(d, m);
}

int search(Position& pos, int alpha, int beta, const int ply, const int depth, PV& pv, Move sm)
{
    assert(pos.is_ok() == 0);
    assert(key_stack.back() == pos.key());
    assert(alpha >= -ScoreMate && alpha < beta && beta <= ScoreMate);
    assert(depth <= DepthMax);

    if (depth <= 0)
        return qsearch(pos, alpha, beta, ply, 0, pv);

    thtable.prefetch(pos.key());
    ehtable.prefetch(pos.key());
    phtable.prefetch(pos.pawn_key());

    global_stats.nodes_search++;

    search_info.seldepth_max = max(ply, search_info.seldepth_max);

    // Do we need to abort the search?
    search_info.check();

    pv[0] = MoveNone;
    
    Node node(pos, ply, depth, alpha, beta, sm);

    PV pv_child;
    int depth_next;

    if (!node.root) {
        if (key_stack_rep(pos) || pos.draw_mat_insuf())
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
    
    if (node.in_check)
        node.eval = -ScoreMate;
    else if (node.tt_eval != ScoreNone)
        node.eval = node.tt_eval;
    else if (move_stack.back() == MoveNull)
        node.eval = -Evals[node.ply - 1] + 2 * TempoBonus;
    else
        node.eval = eval(pos, alpha, beta);

    Evals[ply] = node.eval;

    const bool improving = !node.in_check && (ply < 2 || Evals[ply] > Evals[ply - 2]);

    if (node.root && search_info.move_best != MoveNone) {
        node.tt_move = search_info.move_best;

        assert(node.tt_move != MoveNone);
    }

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
                depth_next = depth - (3 + depth / 3);
               
                const bool null_ok = !tt_hit || node.tt_score >= beta || node.tt_bound < BoundUpper;

                if (null_ok) {
                    NullUndo undo;

                    pos.make_null(undo);
                    int score = -search(pos, -beta, -beta + 1, ply + 1, depth_next, pv_child);
                    pos.unmake_null(undo);

                    if (score >= beta)
                        return score >= mate_in(PliesMax) ? beta : score;
                }
            }
        }
    }

    if (!node.in_check && node.depth <= Z6) {
        int score = node.eval + node.depth * Z11;

        if (score <= alpha) {
            node.score = score;
            node.futile = true;
        }
    }

    MoveList moves;
    MoveList qmoves;
    
    Sort sort(node, History, Killers);

    int move_count = 0;
    MoveUndo undo;

    int sort_score;
    
    for (Move m = sort.next(sort_score); m != MoveNone; m = sort.next(sort_score)) {
        assert(alpha < beta);

        if (m == node.sm || !pos.move_is_legal(m)) continue;

        move_count++;

        if (node.sm && move_count >= Y1) break;
        
        const bool gives_check = pos.move_is_check(m);

        int see = -10000;

        // TODO test with removal of m != node.tt_move
        if (   node.futile
            && m != node.tt_move
            && !gives_check 
            && !m.is_tactical())
            continue;

        // TODO test
        //if (node.futile && !move_is_safe(node, m, see))
        //    continue;

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
            && depth >= Y2
            && !node.sm
            && !node.in_check
            && !score_is_mate(node.tt_score)
            && node.tt_bound <= BoundExact
            && node.tt_depth >= depth - Y3)
        {
            // TODO tune
            const int lbound = node.tt_score - 4 * depth;

            int score = search(pos, lbound, lbound + 1, ply, depth - 4, pv_child, m);

            ext = score <= lbound;
        }

        pos.make_move(m, undo);

        search_info.nodes++;

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
                        search_info.update(depth, node.score, pv, false);
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

    if (node.bs <= -ScoreMate) {
        assert(node.bs == -ScoreMate);

        return max(alpha, mated_in(ply + 1));
    }

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
                const int bonus = -1 - depth * depth / 2;

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
    assert(pos.is_ok(true) == 0);
    assert(key_stack.back() == pos.key());
    assert(depth <= 0);
    assert(alpha >= -ScoreMate && alpha < beta && beta <= ScoreMate);
    
    thtable.prefetch(pos.key());
    ehtable.prefetch(pos.key());
    phtable.prefetch(pos.pawn_key());

    global_stats.nodes_qsearch++;
    
    search_info.seldepth_max = max(ply, search_info.seldepth_max);

    // Do we need to abort the search?
    search_info.check();
    
    Node node(pos, ply, depth, alpha, beta);

    PV pv_child;

    if (node.pv_node)
        pv[0] = MoveNone;

    if (!node.root) {
        if (key_stack_rep(pos) || pos.draw_mat_insuf())
            return ScoreDraw;

        if (ply >= PliesMax)
            return eval(pos, alpha, beta);
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
    
    Sort sort(node, History, Killers);

    int move_count = 0;
    MoveUndo undo;

    int sort_score;

    for (Move m = sort.next(sort_score); m != MoveNone; m = sort.next(sort_score)) {
        assert(alpha < beta);

        int see = -10000;

        if (!node.in_check && depth <= DepthMin && !pos.move_is_recap(m))
            continue;

        if (   !node.in_check 
            && (node.eval + pos.see_max(m) + Z12) <= alpha
            && !(depth == 0 && pos.move_is_check(m)))
            continue;

        if (!node.in_check && !move_is_safe(node, m, see))
            continue;

        if (!pos.move_is_legal(m)) continue;

        pos.make_move(m, undo);
        move_count++;
        search_info.nodes++;
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

int search_iter(Position& pos, int depth_max, PV& pv)
{
    assert(depth_max > 0);

    int score = 0;

    for (int depth = 1; depth <= depth_max; depth++) {
        score = search_aspirate(pos, depth, pv, score);

        assert(pos == search_info.pos);

        search_info.update(depth, score, pv, true);

        if (search_limits.time_limited() && search_limits.time_usage_percent() > 0.46)
            break;
    }

    return score;
}

void search_go()
{
    if (setjmp(search_info.jump_buffer) != 0)
        return;
    
    thtable.reset_stats();
    thtable.inc_gen();

    PV pv;

    Position pos = search_info.pos;

    search_iter(pos, search_limits.depth ? search_limits.depth : DepthMax, pv);

    Stop = true;
}

void search_start()
{
    Stop = false;
    
    SearchInfo& si = search_info;

    UseTransTable       = si.opt_list.get("UseTransTable").check_value();
    KillerMoves         = si.opt_list.get("KillerMoves").check_value();

    NMPruning           = si.opt_list.get("NMPruning").check_value();
    NMPruningDepthMin   = si.opt_list.get("NMPruningDepthMin").spin_value();
    
    SingularExt         = si.opt_list.get("SingularExt").check_value();

    StaticNMP           = si.opt_list.get("StaticNMP").check_value();
    StaticNMPDepthMax   = si.opt_list.get("StaticNMPDepthMax").spin_value();
    StaticNMPFactor     = si.opt_list.get("StaticNMPFactor").spin_value();
    
    Razoring            = si.opt_list.get("Razoring").check_value();
    RazoringMargin      = si.opt_list.get("RazoringMargin").spin_value();
    
    QSearchMargin       = si.opt_list.get("QSearchMargin").spin_value();
    AspMargin           = si.opt_list.get("AspMargin").spin_value();
    TempoBonus          = si.opt_list.get("TempoBonus").spin_value();
    
    UseLazyEval         = si.opt_list.get("UseLazyEval").check_value();
    LazyMargin          = si.opt_list.get("LazyMargin").spin_value();
    
    Y1                  = si.opt_list.get("Y1").spin_value();
    Y2                  = si.opt_list.get("Y2").spin_value();
    Y3                  = si.opt_list.get("Y3").spin_value();
    
    Z0                  = si.opt_list.get("Z0").spin_value();
    Z1                  = si.opt_list.get("Z1").spin_value();
    Z2                  = si.opt_list.get("Z2").spin_value();
    Z3                  = si.opt_list.get("Z3").spin_value();
    Z4                  = si.opt_list.get("Z4").spin_value();
    Z5                  = si.opt_list.get("Z5").spin_value();
    Z6                  = si.opt_list.get("Z6").spin_value();
    Z7                  = si.opt_list.get("Z7").spin_value();
    Z8                  = si.opt_list.get("Z8").spin_value();
    Z9                  = si.opt_list.get("Z9").spin_value();
    Z10                 = si.opt_list.get("Z10").spin_value();
    Z11                 = si.opt_list.get("Z11").spin_value();
    Z12                 = si.opt_list.get("Z12").spin_value();

    search_go();
    
    search_info.uci_bestmove();
}

bool key_stack_rep(const Position& pos)
{
    if (pos.half_moves() >= 100) {
        if (!pos.in_check())
            return true;

        MoveList moves;
        
        return gen_moves(moves, pos, Legal) > 0;
    }

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
    const int orig = m.orig();
    const int dest = m.dest();

    int value = History[node.side][orig][dest];

    value += 32 * bonus - value * abs(bonus) / 128;
    value  = clamp(value, -HistoryMax, HistoryMax);

    History[node.side][orig][dest] = value;
}

void SearchInfo::update(int depth, int score, PV& pv, bool complete)
{
    if (depth <= depth_max)
        return;

    i64 dur = max((i64)1, Util::now() - search_limits.begin);

    if (complete || dur >= 100) {
        ostringstream oss;

        oss << "info"
            << " depth " << depth
            << " seldepth " << search_info.seldepth_max
            << " score " << uci_score(score)
            << " time " << dur
            << " nodes " << nodes
            << " nps " << (i64)(nodes / (dur / 1000.0))
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

    oss << "bestmove " << search_info.move_best.to_string();

    uci_send(oss.str().c_str());
}

void SearchInfo::check()
{
    if (--nodes_countdown > 0)
        return;

    nodes_countdown += SearchInfo::NodesCountdown;

    if (depth_max < 2)
        return;

    bool abort = false;

    i64 dur = Util::now() - search_limits.begin;

    if (Stop)
        abort = true;
    else if (search_limits.nodes && nodes - NodesCountdown >= search_limits.nodes)
        abort = true;
    else if (search_limits.depth && depth_max >= search_limits.depth)
        abort = true;
    else if (search_limits.movetime && dur >= search_limits.movetime)
        abort = true;
    else if (search_limits.time) {
        const i64 ntime     = search_limits.time_min;
        const i64 xtime     = search_limits.time_max;
        const i64 delta     = xtime - ntime;
        const i64 margin    = delta * pow(0.9, stability);

        if (dur >= ntime + margin)
            abort = true;
    }
   
    if (abort)
        longjmp(jump_buffer, 1);
}
