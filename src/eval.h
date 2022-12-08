#ifndef EVAL_H
#define EVAL_H

#include <bit>
#include "htable.h"
#include "pos.h"
#include "types.h"

// Constants

constexpr int ScoreDraw     =             0;
constexpr int ScoreMate     =         32000;
constexpr int ScoreNone     = ScoreMate + 1;

extern Value ValuePawn;
extern Value ValueKnight;
extern Value ValueBishop;
extern Value ValueRook;
extern Value ValueQueen;
extern Value ValueKing;

const Value ValuePiece[6] = {
    ValuePawn,
    ValueKnight,
    ValueBishop,
    ValueRook,
    ValueQueen,
    ValueKing
};

extern i16 PSQT[PhaseCount][12][128];

struct EvalEntry {
    static constexpr std::size_t KeyBits = 48;

    u64 key : 48;
    i64 score : 16; // replace with mg and eg values?
};

static_assert(sizeof(EvalEntry) == 8);

extern HashTable<16 * 1024, EvalEntry> ehtable;

void eval_init();

int eval(const Position& pos, int alpha, int beta);

void eval_tune(int argc, char* argv[]);

#endif
