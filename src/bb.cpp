#include <sstream>
#include <string>
#include <cassert>
#include <cstdlib>
#include "bb.h"
#include "misc.h"
#include "square.h"

using namespace std;

namespace bb {

u64 PawnSpan[2][64];
u64 PawnSpanAdj[2][64];

void init()
{
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;

        PawnSpan[0][sq] = Files[file] & RanksGT[rank];
        PawnSpan[1][sq] = Files[file] & RanksLT[rank];
        
        PawnSpanAdj[0][sq] = FilesAdj[file] & RanksGT[rank];
        PawnSpanAdj[1][sq] = FilesAdj[file] & RanksLT[rank];
    }
}

int lsb(u64 bb)
{
    return std::countr_zero(bb);
}

u64 bit(int sq)
{
    assert(sq64_is_ok(sq));

    return 1ull << sq;
}

u64 bit88(int sq)
{
    assert(sq88_is_ok(sq));
    
    return bit(to_sq64(sq));
}

int pop(u64& bb)
{
    assert(bb);

    int sq = std::countr_zero(bb);

    bb &= (bb - 1);

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

u64 set(u64 bb, int i)
{
    return bb |= (1ull << i);
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
