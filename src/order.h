#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <limits>
#include <cassert>
#include "history.h"
#include "move.h"
#include "search.h"


struct OMove {

    static constexpr int Offset = 100000;

    static u64 make(Move m, int see, int score)
    {
        assert(see >= 0 && see <= 2);

        score = std::clamp(score + Offset, 0, 2 * Offset);

        u64 ret =  u64(m)
                | (u64(see)   << 32)
                | (u64(score) << 34);

        return ret;
    }

    static Move move (u64 data) { return u32(data); }
    static int  see  (u64 data) { return int(data >> 32) & 0x3; }
    static int  score(u64 data) { return int(data >> 34) - Offset; }

};

struct Order {
    static constexpr int ScoreTT        =  1 << 16;
    static constexpr int ScoreTactical  =  1 << 15;
    static constexpr int ScoreSpecial   =  1 << 14;

    Order(const Node& node, History& history);

    Move next();

    bool singular() const;
    int& see(const Position& pos, bool calc);
    int score() const;

    static bool special(int score);

private:
    std::size_t index_ = 0;

    Move move_;
    int see_;
    int score_;

    std::size_t count_ = 0;
    u64 omoves_[128];
};

#endif
