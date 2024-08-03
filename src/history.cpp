#include <limits>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "history.h"
#include "misc.h"
#include "move.h"
#include "pos.h"
#include "search.h"
using namespace std;

void History::clear()
{
    memset(counters_, 0, sizeof(counters_));
    memset(killers_,  0, sizeof(killers_));
    memset(quiets_,   0, sizeof(quiets_));
    memset(cont_,     0, sizeof(cont_));
}

void History::update(const Node& node, const MoveList& qmoves)
{
    Move best_move = node.best_move;

    assert(!best_move.is_tactical());

    int depth = min(node.depth, 10);
    int bonus = 4 * (depth * depth + 40 * depth - 30);

    if (Move pm = node.pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = node.pos.board<12>(dest);

        assert(piece12_is_ok(piece));

        // Counter moves

        counters_[pm.is_capture()][piece][dest] = best_move;

        // Continuation heuristic

        update(cont_ptr(node.pos, best_move), bonus);

        for (const Move& m : qmoves)
            if (m != best_move)
                update(cont_ptr(node.pos, m), -bonus);
    }

    side_t side = node.pos.side();

    int ply = node.ply;

    // Killer moves
    
    if (killers_[side][ply][0] != best_move) {
        killers_[side][ply][1] = killers_[side][ply][0];
        killers_[side][ply][0] = best_move;
    }

    // Quiet heuristic

    update(quiet_ptr(side, best_move), bonus);

    for (const Move& m : qmoves)
        if (m != best_move)
            update(quiet_ptr(side, m), -bonus);
}

void History::specials(const Node& node, Move& killer1, Move& killer2, Move& counter) const
{
    side_t side = node.pos.side();

    int ply = node.ply;

    killer1 = killers_[side][ply][0];
    killer2 = killers_[side][ply][1];

    if (Move pm = node.pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = node.pos.board<12>(dest);

        assert(piece12_is_ok(piece));

        counter = counters_[pm.is_capture()][piece][dest];
    }
}

void History::clear_killers(side_t side, int ply)
{
    killers_[side][ply][0] = MoveNone;
    killers_[side][ply][1] = MoveNone;
}

int History::quiet_score(side_t side, const Move& m)
{
    assert(!m.is_tactical());

    return quiets_[m.index(side)];
}

i16 * History::quiet_ptr(side_t side, const Move& m)
{
    assert(!m.is_tactical());

    return &quiets_[m.index(side)];
}

int History::cont_score(const Position& pos, const Move& m)
{
    i16 * p = cont_ptr(pos, m);

    return p ? *p : 0;
}

i16 * History::cont_ptr(const Position& pos, const Move& m)
{
    assert(!m.is_tactical());

    Move pm = pos.prev_move();

    if (!pm.is_valid()) [[unlikely]]
        return nullptr;

    int mdest =  m.dest();
    int odest = pm.dest();

    int mtype6  = pos.board< 6>(mdest);
    int otype12 = pos.board<12>(odest);

    return &cont_[pm.is_capture()][otype12][odest][mtype6][mdest];
}

int History::score(const Position& pos, const Move& m)
{
    assert(!m.is_tactical());

    return quiet_score(pos.side(), m) + cont_score(pos, m);
}

void History::update(i16 * p, int bonus)
{
    assert(p != nullptr);

    *p += bonus - *p * abs(bonus) / 16384;
}
