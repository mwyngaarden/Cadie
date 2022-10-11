#ifndef GEN_H
#define GEN_H

#include "move.h"
#include "piece.h"
#include "pos.h"
#include "types.h"

enum GenType { Pseudo, Legal, Tactical };

extern u8 CastleLUT[128];

constexpr int KnightIncrs[8]    = { -33, -31, -18, -14, 14, 18, 31, 33 };
constexpr int BishopIncrs[4]    = { -17, -15,  15,  17 };
constexpr int RookIncrs[4]      = { -16,  -1,   1,  16 };
constexpr int QueenIncrs[8]     = { -17, -16, -15,  -1, 1, 15, 16, 17 };

void gen_init();

std::size_t gen_moves(MoveList& moves, const Position& pos, GenType gtype);

int  delta_incr   (int orig, int dest);
bool pseudo_attack(int orig, int dest, u8 piece);
u8   pseudo_attack(int incr);

#endif
