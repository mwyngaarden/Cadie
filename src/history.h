#ifndef HISTORY_H
#define HISTORY_H

#include <limits>
#include <vector>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "pos.h"
#include "search.h"


class History {
public:
    static constexpr int HistoryMax = 16384;

    void reset();
    void update(const Position& pos, int ply, int depth, Move best_move, const MoveList& quiets);
    void specials(const Position& pos, int ply, Move& killer1, Move& killer2, Move& counter) const;
    void reset_killers(Side sd, int ply);

    int score(const Position& pos, Move m) const;

private:

    i16 * cont_ptr(const Position& pos, Move m);

    void update(i16 * p, int bonus);

    i16 cont_[2][12][64][6][64];
    i16 quiets_[8192];

    Move killers_[2][PliesMax + 2][2];
    Move counters_[2][12][64];
};

#endif
