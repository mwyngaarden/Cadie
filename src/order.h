#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <limits>
#include <cassert>
#include "history.h"
#include "move.h"
#include "search.h"


struct OMove {
    static constexpr int Offset = 1 << 28;

    static u64 make(Move m, int see, int score)
    {
        assert(m.is_valid());

        return (u64(score + Offset) << 34) | (u64(see) << 32) | u64(m);
    }

    static Move move (u64 data) { return u32(data); }
    static int  see  (u64 data) { return int(data >> 32) & 0x3; }
    static int  score(u64 data) { return int(data >> 34) - Offset; }

};

struct Order {
    static constexpr int ScoreTT        =  1 << 26;
    static constexpr int ScoreTactical  =  1 << 25;
    static constexpr int ScoreKiller1   = (1 << 24) + 2;
    static constexpr int ScoreKiller2   = (1 << 24) + 1;
    static constexpr int ScoreCounter   = (1 << 24) + 0;

    Order(const Node& node, History& history);

    Move next();

    bool singular() const;
    int& see(const Position& pos, bool calc);
    int score() const;

    static bool is_special(int score);

private:
    std::size_t index_ = 0;

    Move move_;
    int see_;
    int score_;

    std::size_t count_ = 0;
    u64 omoves_[128];
};

#endif
