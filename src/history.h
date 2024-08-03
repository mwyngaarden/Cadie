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

    void clear();
    void update(const Node& node, const MoveList& qmoves);
    void specials(const Node& node, Move& killer1, Move& killer2, Move& counter) const;
    void clear_killers(side_t side, int ply);

    int score(const Position& pos, const Move& m);

private:

    int quiet_score(side_t side, const Move& m);
    int cont_score(const Position& pos, const Move& m);

    i16 * quiet_ptr(side_t side, const Move& m);
    i16 * cont_ptr(const Position& pos, const Move& m);

    void update(i16 * p, int bonus);

    i16 cont_[2][12][64][6][64];
    i16 quiets_[8192]; // 2 x 64 x 64

    Move killers_[2][PliesMax + 2][2];
    Move counters_[2][12][64];
};

#endif
