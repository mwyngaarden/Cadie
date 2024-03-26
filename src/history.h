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
    static constexpr int HistoryMax = DepthMax * DepthMax;

    void clear();
    void update(const Node& node, const MoveList& qmoves, const MoveList& cmoves);
    
    int special_index(const Position& pos, int ply, const Move& m) const;

    void clear_killers(side_t side, int ply);
    
    int quiet_score(const Position& pos, const Move& m);
    int capture_score(const Position& pos, const Move& m);

private:

    i16 * quiet_ptr(const Position& pos, const Move& m);
    i16 * capture_ptr(const Position& pos, const Move& m);

    void update(i16 * p, int bonus);

    i16 quiets_[2][64][64];
    i16 captures_[6][64][6];
    Move killers_[6][PliesMax][2];
    Move counters_[6][64];
};

#endif
