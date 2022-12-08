#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <cstdint>
#include "types.h"

void zobrist_init();
void zobrist_validate();

u64 zobrist_piece(int piece, int sq);
u64 zobrist_castle(u8 flags);
u64 zobrist_ep(int sq);
u64 zobrist_side(side_t side);

#endif
