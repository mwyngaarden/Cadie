#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <limits>
#include "history.h"
#include "move.h"
#include "search.h"


struct ExtMove {
    static constexpr int Offset = 1 << 30;

    static u64 make(Move m, int score)
    {
        return (u64(score + Offset) << 32) | u64(m);
    }

    static Move move(u64 data) { return u32(data); }
    static int score(u64 data) { return int(data >> 32) - Offset; }

};

struct Order {
    static constexpr int ScoreTT        =  1 << 26;
    static constexpr int ScoreTactical  =  1 << 25;
    static constexpr int ScoreKiller1   = (1 << 24) + 2;
    static constexpr int ScoreKiller2   = (1 << 24) + 1;
    static constexpr int ScoreCounter   = (1 << 24) + 0;

    Order(Position& pos, const History& history, Move best_move, int ply, int depth);
    Order(Position& pos, Move best_move);

    Move next();

    bool singular() const;
    int see();
    int score() const;

private:
    std::size_t index_ = 0;

    Move move_;
    int see_;
    int score_;

    std::size_t count_ = 0;
    u64 emoves_[128];

    Position& pos_;
};

#endif
