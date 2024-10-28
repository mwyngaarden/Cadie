#ifndef EVAL_H
#define EVAL_H

#include <bit>
#include <filesystem>
#include "ht.h"
#include "misc.h"
#include "pos.h"


constexpr int ScoreDraw     =     0;
constexpr int ScoreMate     = 32000;
constexpr int ScoreMateMin  = ScoreMate - PliesMax;
constexpr int ScoreNone     = ScoreMate + 1;

extern int TempoB;

struct EvalEntry {
    using Lock = u32;

    static constexpr std::size_t LockBits = sizeof(Lock) * 8;

    Lock lock;
    i16 score;
    i16 pad;
};

static_assert(sizeof(EvalEntry) == 8);

extern HashTable<64 * 1024 * 1024, EvalEntry> etable;

int eval(const Position& pos);

void eval_init();

#ifdef TUNE
void eval_tune(int argc, char* argv[]);
#endif

#endif
