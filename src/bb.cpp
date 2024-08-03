#include <sstream>
#include <string>
#include <cassert>
#include <cstdlib>
#include "attack.h"
#include "bb.h"
#include "gen.h"
#include "misc.h"
#include "square.h"

using namespace std;

namespace bb {

u64 InBetween[64][64];

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

    for (int i = 0; i < 64; i++) {
        int sq1 = to_sq88(i);

        for (int j = 0; j < 64; j++) {
            if (i == j) continue;

            int sq2 = to_sq88(j);

            //if (!bb::test(QueenAttacks[to_sq64(sq1)], to_sq64(sq2))) continue;
            
            if (!pseudo_attack(sq2, sq1, QueenFlags256)) continue;

            int incr = delta_incr(sq2, sq1);

            for (sq2 += incr; sq2 != sq1; sq2 += incr)
                InBetween[i][j] |= bb::bit(to_sq64(sq2));
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

    bb &= (bb - 1);

    return sq;
}

int poprev(u64& bb)
{
    assert(bb);

    int sq = msb(bb);

    bb ^= 1ull << sq;

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

int pawns2(u64 bb)
{
    return bool(bb) + bb::multi(bb);
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
