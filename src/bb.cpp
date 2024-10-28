#include <sstream>
#include <string>
#include <cassert>
#include <cstdlib>
#include "attacks.h"
#include "bb.h"
#include "gen.h"
#include "misc.h"
#include "square.h"

using namespace std;

namespace bb {

u64 Between[64][64];

u64 PawnSpan[2][64];
u64 PawnSpanAdj[2][64];

void init()
{
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;

        PawnSpan[White][sq] = Files[file] & RanksForward[White][rank];
        PawnSpan[Black][sq] = Files[file] & RanksForward[Black][rank];

        PawnSpanAdj[White][sq] = FilesAdj[file] & RanksForward[White][rank];
        PawnSpanAdj[Black][sq] = FilesAdj[file] & RanksForward[Black][rank];
    }

    for (int i = 0; i < 64; i++) {
        u64 qatt = QueenAttacks[i];

        for (int j = 0; j < 64; j++) {
            if (!bb::test(qatt, j)) continue;

            int incr = delta_incr(j, i);

            for (int k = j + incr; k != i; k += incr)
                Between[i][j] |= bb::bit(k);
        }
    }
}

int lsb(u64 bb)
{
    return std::countr_zero(bb);
}

int msb(u64 bb)
{
    return 63 - std::countl_zero(bb);
}

int pop(u64& bb)
{
    assert(bb);

    int sq = lsb(bb);

    bb &= bb - 1;

    return sq;
}

bool test(u64 bb, int i)
{
    return bb & (1ull << i);
}

bool single(u64 bb)
{
    return bb && (bb & (bb - 1)) == 0;
}

bool multi(u64 bb)
{
    return bb && (bb & (bb - 1)) != 0;
}

u64 set(u64 bb, int i)
{
    return bb | (1ull << i);
}

u64 reset(u64 bb, int i)
{
    return bb & ~(1ull << i);
}

u64 flip(u64 bb, int i)
{
    return bb ^ (1ull << i);
}

u64 PawnAttacks(u64 pawns, side_t side)
{
    return side == White ? ((pawns & ~FileH) << 9) | ((pawns & ~FileA) << 7)
                         : ((pawns & ~FileH) >> 7) | ((pawns & ~FileA) >> 9);
}

u64 PawnSingles(u64 pawns, u64 empty, side_t side)
{
    return empty & (side == White ? pawns << 8 : pawns >> 8);
}

u64 PawnDoubles(u64 pawns, u64 empty, side_t side)
{
    pawns = PawnSingles(pawns, empty, side);
    pawns = PawnSingles(pawns, empty, side);

    return pawns & (side == White ? Rank4 : Rank5);
}

string dump(u64 occ)
{
    ostringstream oss;

    for (int i = 56; i >= 0; i -= 8) {
        for (int j = 0; j < 8; j++) {
            if (j) oss << ' ';

            oss << (test(occ, i + j) ? '1' : '0');
        }
        
        if (i) oss << endl;
    }

    return oss.str();
}

}
