#ifndef PERFT_H
#define PERFT_H

#include <array>
#include <vector>
#include <cstdint>
#include "misc.h"

void perft(int argc, char* argv[]);

i64 perft(int depth, i64& illegal_moves, i64& total_microseconds, i64& total_cycles, bool startpos);

#endif
