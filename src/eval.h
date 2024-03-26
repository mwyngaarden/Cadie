#ifndef EVAL_H
#define EVAL_H

#include <bit>
#include <filesystem>
#include "ht.h"
#include "misc.h"
#include "pos.h"

struct Tuner {
    double time         =     0;
    std::size_t seed    =     0;
    double ratio        =     1;

    bool bench          = false;
    bool ktune          = false;
    bool opt            = false;
    bool debug          = false;

    std::filesystem::path path;
};

constexpr int ScoreDraw     =     0;
constexpr int ScoreMate     = 32000;
constexpr int ScoreMateMin  = ScoreMate - PliesMax;
constexpr int ScoreNone     = ScoreMate + 1;

struct EvalEntry {
    static constexpr std::size_t LockBits = 32;

    u32 lock;
    i16 score;
    i16 pad;
};

static_assert(sizeof(EvalEntry) == 8);

const std::size_t EHSizeMBDefault = 16;

extern HashTable<EHSizeMBDefault * 1024, EvalEntry> etable;

int eval(const Position& pos, int ply = 0);

void eval_init();

#ifdef TUNE
void eval_tune(int argc, char* argv[]);
#endif

#endif
