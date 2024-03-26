#ifndef PAWN_H
#define PAWN_H

#include <cstdint>
#include "ht.h"
#include "misc.h"
#include "pos.h"

extern int PawnBackOpenPm;
extern int PawnBackOpenPe;
extern int PawnBackwardPm;
extern int PawnBackwardPe;
extern int PawnCandidateBaseMg;
extern int PawnCandidateBaseEg;
extern int PawnCandidateFactorMg;
extern int PawnCandidateFactorEg;
extern int PawnDoubledPm;
extern int PawnDoubledPe;
extern int PawnIsoOpenPm;
extern int PawnIsoOpenPe;
extern int PawnIsolatedPm;
extern int PawnIsolatedPe;

extern int PawnPassedBaseMg;
extern int PawnPassedBaseEg;
extern int PawnPassedFactorMg;
extern int PawnPassedFactorEg;

extern int PawnPassedFactorA;
extern int PawnPassedFactorB;
extern int PawnPassedFactorC;

extern int PawnAttackMg;
extern int PawnAttackEg;

struct PawnEntry {
    static constexpr std::size_t LockBits = 32;

    u32 lock    = 0;
    i16 mg      = 0;
    i16 eg      = 0;
    u64 passed  = 0;
};

static_assert(sizeof(PawnEntry) == 16);

constexpr std::size_t PHSizeMBDefault = 1;

extern HashTable<PHSizeMBDefault * 1024, PawnEntry> ptable;

Value eval_passers  (const Position& pos, PawnEntry& pentry);
Value eval_pawns    (const Position& pos, PawnEntry& pentry);

#endif
