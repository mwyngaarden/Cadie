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
    depth = min(depth, 10);

    int bonus = 4 * (depth * depth + 40 * depth - 30);

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = pos.square(dest);

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

    Side sd = pos.side();

    // Killer moves
    
    if (killers_[sd][ply][0] != best_move) {
        killers_[sd][ply][1] = killers_[sd][ply][0];
        killers_[sd][ply][0] = best_move;
    }

    // Quiet heuristic

    if (quiets.size() > 1 || depth > 1) {
        update(&quiets_[best_move.index(sd)], bonus);

        for (Move m : quiets)
            if (m != best_move)
                update(&quiets_[m.index(sd)], -bonus);
    }
}

void History::specials(const Position& pos, int ply, Move& killer1, Move& killer2, Move& counter) const
{
    Side sd = pos.side();

    killer1 = killers_[sd][ply][0];
    killer2 = killers_[sd][ply][1];

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int dest = pm.dest();
        int piece = pos.square(dest);

        counter = counters_[pm.is_capture()][piece][dest];
    }
}

void History::reset_killers(Side sd, int ply)
{
    killers_[sd][ply][0] = Move::None();
    killers_[sd][ply][1] = Move::None();
}

i16 * History::cont_ptr(const Position& pos, Move m)
{
    Move pm = pos.prev_move();

    int morig =  m.orig();
    int mdest =  m.dest();
    int odest = pm.dest();
    bool cap  = pm.is_capture();

    int mtype6 = pos.square(morig) / 2;
    int otype12 = pos.square(odest);

    return &cont_[cap][otype12][odest][mtype6][mdest];
}

int History::score(const Position& pos, Move m) const
{
    int score = quiets_[m.index(pos.side())];

    if (Move pm = pos.prev_move(); pm.is_valid()) {
        int morig =  m.orig();
        int mdest =  m.dest();
        int odest = pm.dest();

        int mtype6 = pos.square(morig) / 2;
        int otype12 = pos.square(odest);

        score += cont_[pm.is_capture()][otype12][odest][mtype6][mdest];
    }

    return score;
}

void History::update(i16 * p, int bonus)
{
    *p += bonus - *p * abs(bonus) / HistoryMax;
}
