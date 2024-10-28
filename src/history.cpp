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

void History::reset()
{
    memset(counters_, 0, sizeof(counters_));
    memset(killers_,  0, sizeof(killers_));
    memset(quiets_,   0, sizeof(quiets_));
    memset(cont_,     0, sizeof(cont_));
}

void History::update(const Position& pos, int ply, int depth, Move best_move, const MoveList& quiets)
{
    assert(!best_move.is_tactical());

    depth = min(depth, 10);

    int bonus = 4 * (depth * depth + 40 * depth - 30);

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = pos.board<12>(dest);

        assert(piece12_is_ok(piece));

        // Counter moves

        counters_[pm.is_capture()][piece][dest] = best_move;

        // Continuation heuristic

        if (quiets.size() > 1 || depth > 1) {
            update(cont_ptr(pos, best_move), bonus);

            for (Move m : quiets)
                if (m != best_move)
                    update(cont_ptr(pos, m), -bonus);
        }
    }

    side_t side = pos.side();

    // Killer moves
    
    if (killers_[side][ply][0] != best_move) {
        killers_[side][ply][1] = killers_[side][ply][0];
        killers_[side][ply][0] = best_move;
    }

    // Quiet heuristic

    if (quiets.size() > 1 || depth > 1) {
        update(&quiets_[best_move.index(side)], bonus);

        for (Move m : quiets)
            if (m != best_move)
                update(&quiets_[m.index(side)], -bonus);
    }
}

void History::specials(const Position& pos, int ply, Move& killer1, Move& killer2, Move& counter) const
{
    side_t side = pos.side();

    killer1 = killers_[side][ply][0];
    killer2 = killers_[side][ply][1];

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = pos.board<12>(dest);

        assert(piece12_is_ok(piece));

        counter = counters_[pm.is_capture()][piece][dest];
    }
}

void History::reset_killers(side_t side, int ply)
{
    killers_[side][ply][0] = Move::None();
    killers_[side][ply][1] = Move::None();
}

i16 * History::cont_ptr(const Position& pos, Move m)
{
    assert(pos.prev_move().is_valid());
    assert(!m.is_tactical());

    Move pm = pos.prev_move();

    int morig =  m.orig();
    int mdest =  m.dest();
    int odest = pm.dest();
    bool cap  = pm.is_capture();

    int mtype6 = pos.board<6>(morig);
    int otype12 = pos.board<12>(odest);

    return &cont_[cap][otype12][odest][mtype6][mdest];
}

int History::score(const Position& pos, Move m) const
{
    assert(!m.is_tactical());

    int score = quiets_[m.index(pos.side())];

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int morig =  m.orig();
        int mdest =  m.dest();
        int odest = pm.dest();

        int mtype6 = pos.board<6>(morig);
        int otype12 = pos.board<12>(odest);

        score += cont_[pm.is_capture()][otype12][odest][mtype6][mdest];
    }

    return score;
}

void History::update(i16 * p, int bonus)
{
    assert(p != nullptr);

    *p += bonus - *p * abs(bonus) / 16384;
}
