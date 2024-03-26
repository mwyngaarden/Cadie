#ifndef ATTACK_H
#define ATTACK_H

#include "misc.h"
#include "move.h"
#include "piece.h"
#include "pos.h"

extern u64 KingZone[64];
extern u64 PawnAttacks[2][64];
extern u64 PieceAttacks[6][64];
extern u64 InBetween[64][64];

u64 attackers_to(const Position& pos, u64 occ, int dest);

void attack_init();

#endif
