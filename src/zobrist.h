#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <cstdint>
#include "misc.h"

namespace zob {

void init();

u64 piece(int piece, int sq);
u64 castle(u8 flags);
u64 ep(int sq);
u64 side();

}

#endif
