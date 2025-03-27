#include <sstream>
#include <string>
#include <cstdlib>
#include "attacks.h"
#include "bb.h"
#include "gen.h"
#include "misc.h"
#include "square.h"

using namespace std;

namespace bb {

int Dist[64][64];
u64 Line[64][64];

u64 Between[64][64];

u64 PawnSpan[2][64];
u64 PawnSpanAdj[2][64];

u64 KingZone[64];

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
        int f = clamp(square::file(i), 1, 6);
        int r = clamp(square::rank(i), 1, 6);

        int sq = to_sq(f, r);

        KingZone[i] = KingAttacks[sq] | bb::bit(sq);
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

    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            Dist[i][j] = square::dist(i, j);

    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 64; j++) {
            if (i == j) continue;

            if (square::file_eq(i, j)) Line[i][j] = bb::Files64[i];
            if (square::rank_eq(i, j)) Line[i][j] = bb::Ranks64[i];

            if (square::diag_eq(i, j)) Line[i][j] = bb::Diags[square::diag(i)];
            if (square::anti_eq(i, j)) Line[i][j] = bb::Antis[square::anti(i)];
        }
    }
}

u64 PawnAttacks(Side sd, u64 pawns)
{
    return sd == White ? ((pawns & ~FileH) << 9) | ((pawns & ~FileA) << 7)
                       : ((pawns & ~FileH) >> 7) | ((pawns & ~FileA) >> 9);
}

u64 PawnAttacksSpan(Side sd, u64 pawns)
{
    u64 span = PawnAttacks(sd, pawns);

    span |= sd == White ? span <<  8 : span >>  8;
    span |= sd == White ? span << 16 : span >> 16;
    span |= sd == White ? span << 32 : span >> 32;

    return span;
}

u64 PawnAttacksDouble(Side sd, u64 pawns)
{
    return sd == White ? ((pawns & ~FileH) << 9) & ((pawns & ~FileA) << 7)
                       : ((pawns & ~FileH) >> 7) & ((pawns & ~FileA) >> 9);
}

u64 PawnSingles(Side sd, u64 pawns, u64 empty)
{
    return empty & (sd == White ? pawns << 8 : pawns >> 8);
}

u64 PawnDoubles(Side sd, u64 pawns, u64 empty)
{
    pawns = PawnSingles(sd, pawns, empty);
    pawns = PawnSingles(sd, pawns, empty);

    return pawns & (sd == White ? Rank4 : Rank5);
}

u64 KnightAttacks(u64 knights)
{
    u64 l1 = (knights >> 1) & ~FileH;
    u64 l2 = (knights >> 2) & ~(FileG | FileH);
    u64 r1 = (knights << 1) & ~FileA;
    u64 r2 = (knights << 2) & ~(FileA | FileB);

    u64 h1 = l1 | r1;
    u64 h2 = l2 | r2;

    return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
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
