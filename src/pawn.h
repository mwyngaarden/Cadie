#ifndef PAWN_H
#define PAWN_H

#include <cstdint>
#include "ht.h"
#include "misc.h"
#include "pos.h"

extern Value PawnDoubledPenalty;
extern Value PawnBackwardPenalty;
extern Value PawnBackOpenPenalty;
extern Value PawnIsolatedPenalty;
extern Value PawnIsoOpenPenalty;

extern Value PawnPassedFactor1;
extern Value PawnPassedFactor2;
extern Value PawnPassedBase;
extern Value PawnCandidateFactor;
extern Value PawnCandidateBase;

struct PawnEntry {
    static constexpr std::size_t LockBits = 32;

    u32 lock    = 0;
    i16 mg      = 0;
    i16 eg      = 0;
    u64 passed  = 0;
};

static_assert(sizeof(PawnEntry) == 16);

constexpr std::size_t PHSizeMBDefault = 1;

extern HT<PHSizeMBDefault * 1024, PawnEntry> ptable;

Value eval_passers  (const Position& pos, PawnEntry& pentry);
Value eval_pawns    (const Position& pos, PawnEntry& pentry);

#endif
