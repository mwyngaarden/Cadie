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
extern int PawnDoubledPm;
extern int PawnDoubledPe;
extern int PawnIsoOpenPm;
extern int PawnIsoOpenPe;
extern int PawnIsolatedPm;
extern int PawnIsolatedPe;

extern int PawnPassedB[2][3][8];
extern int PawnPassedGaurdedBm;
extern int PawnPassedGaurdedBe;

extern int PawnAttackB[2][6];

extern int PawnCandidateB[8];
extern int PawnConnectedB[8];
extern int PawnConnectedPB;

Value eval_pawns(const Position& pos);

#endif
