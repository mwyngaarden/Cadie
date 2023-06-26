#ifndef EVAL_H
#define EVAL_H

#include <bit>
#include "ht.h"
#include "misc.h"
#include "pos.h"

// Constants

constexpr int ScoreDraw     =     0;
constexpr int ScoreMate     = 32000;
constexpr int ScoreMateMin  = 32000 - PliesMax;
constexpr int ScoreNone     = 32001;

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

struct EvalTerms {
    int mob[2][2];
    int psqt[2][2];
    int att[2][2];
    int mat[2][2];
    int ksaf[2][2];
    int ktrp[2][2];
    int pstr[2][2];
};


struct EvalEntry {
    static constexpr std::size_t LockBits = 32;

    u32 lock;
    i16 score;
    i16 pad;
};

static_assert(sizeof(EvalEntry) == 8);

const std::size_t EHSizeMBDefault = 16;

extern HT<EHSizeMBDefault * 1024, EvalEntry> etable;

void eval_init();

int eval(const Position& pos, int ply = 0);

void eval_tune(int argc, char* argv[]);

#endif
