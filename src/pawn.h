#ifndef PAWN_H
#define PAWN_H

#include <cstdint>
#include "htable.h"
#include "pos.h"
#include "types.h"

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
    static constexpr std::size_t KeyBits = 32;

    u32 key     = 0;
    i16 mg      = 0;
    i16 eg      = 0;
    u64 passed  = 0;
};

static_assert(sizeof(PawnEntry) == 16);

extern HashTable<1024, PawnEntry> phtable;

Value eval_passers  (const Position& pos, PawnEntry& pentry);
Value eval_pawns    (const Position& pos, PawnEntry& pentry);

#endif
