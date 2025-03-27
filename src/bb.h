#ifndef BB_H
#define BB_H

#include <bit>
#include <string>
#include "attacks.h"
#include "misc.h"
#include "piece.h"
#include "square.h"

namespace bb {

void init();

extern int Dist[64][64];
extern u64 Line[64][64];

extern u64 Between[64][64];

extern u64 PawnSpan[2][64];
extern u64 PawnSpanAdj[2][64];

extern u64 KingZone[64];

constexpr u64 Rank1 = 0x00000000000000ffull;
constexpr u64 Rank2 = Rank1 << 8;
constexpr u64 Rank3 = Rank2 << 8;
constexpr u64 Rank4 = Rank3 << 8;
constexpr u64 Rank5 = Rank4 << 8;
constexpr u64 Rank6 = Rank5 << 8;
constexpr u64 Rank7 = Rank6 << 8;
constexpr u64 Rank8 = Rank7 << 8;

constexpr u64 FileA = 0x0101010101010101ull;
constexpr u64 FileB = FileA << 1;
constexpr u64 FileC = FileB << 1;
constexpr u64 FileD = FileC << 1;
constexpr u64 FileE = FileD << 1;
constexpr u64 FileF = FileE << 1;
constexpr u64 FileG = FileF << 1;
constexpr u64 FileH = FileG << 1;

constexpr u64 Full       = 0xffffffffffffffffull;
constexpr u64 QueenSide  = FileA | FileB | FileC | FileD;
constexpr u64 KingSide   = ~QueenSide;
constexpr u64 PromoRanks = Rank1 | Rank8;
constexpr u64 PawnRanks  = ~PromoRanks;
constexpr u64 Light      = 0x55aa55aa55aa55aaull;
constexpr u64 Dark       = ~Light;
constexpr u64 Corners    = 0x8100000000000081;
constexpr u64 Center4    = (FileD | FileE) & (Rank4 | Rank5);
constexpr u64 Center16   = (FileC | FileD | FileE | FileF) & (Rank3 | Rank4 | Rank5 | Rank6);

constexpr u64 Outposts[2] = {
    ~(FileA | FileH) & (Rank4 | Rank5 | Rank6),
    ~(FileA | FileH) & (Rank3 | Rank4 | Rank5)
};

constexpr u64 Ranks[8] = { Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 };
constexpr u64 Files[8] = { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH };

constexpr u64 Files64[64] = {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH,
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH
};

constexpr u64 Ranks64[64] = {
    Rank1, Rank1, Rank1, Rank1, Rank1, Rank1, Rank1, Rank1,
    Rank2, Rank2, Rank2, Rank2, Rank2, Rank2, Rank2, Rank2,
    Rank3, Rank3, Rank3, Rank3, Rank3, Rank3, Rank3, Rank3,
    Rank4, Rank4, Rank4, Rank4, Rank4, Rank4, Rank4, Rank4,
    Rank5, Rank5, Rank5, Rank5, Rank5, Rank5, Rank5, Rank5,
    Rank6, Rank6, Rank6, Rank6, Rank6, Rank6, Rank6, Rank6,
    Rank7, Rank7, Rank7, Rank7, Rank7, Rank7, Rank7, Rank7,
    Rank8, Rank8, Rank8, Rank8, Rank8, Rank8, Rank8, Rank8
};

constexpr u64 RanksForward[2][8] = {
    {
        Full <<  8,
        Full << 16,
        Full << 24,
        Full << 32,
        Full << 40,
        Full << 48,
        Full << 56,
        0
    },
    {
        0,
        Full >> 56,
        Full >> 48,
        Full >> 40,
        Full >> 32,
        Full >> 24,
        Full >> 16,
        Full >>  8
    }
};

constexpr u64 FilesAdj[8] = {
    FileB,
    FileA | FileC,
    FileB | FileD,
    FileC | FileE,
    FileD | FileF,
    FileE | FileG,
    FileF | FileH,
    FileG
};

constexpr u64 Diags[15] = {
    0x0000000000000080ull,
    0x0000000000008040ull,
    0x0000000000804020ull,
    0x0000000080402010ull,
    0x0000008040201008ull,
    0x0000804020100804ull,
    0x0080402010080402ull,
    0x8040201008040201ull,
    0x4020100804020100ull,
    0x2010080402010000ull,
    0x1008040201000000ull,
    0x0804020100000000ull,
    0x0402010000000000ull,
    0x0201000000000000ull,
    0x0100000000000000ull
};

constexpr u64 Antis[15] = {
    0x0000000000000001ull,
    0x0000000000000102ull,
    0x0000000000010204ull,
    0x0000000001020408ull,
    0x0000000102040810ull,
    0x0000010204081020ull,
    0x0001020408102040ull,
    0x0102040810204080ull,
    0x0204081020408000ull,
    0x0408102040800000ull,
    0x0810204080000000ull,
    0x1020408000000000ull,
    0x2040800000000000ull,
    0x4080000000000000ull,
    0x8000000000000000ull
};

constexpr uint64_t Diags64[64] = {
    0x8040201008040201ull, 0x0080402010080402ull, 0x0000804020100804ull, 0x0000008040201008ull,
    0x0000000080402010ull, 0x0000000000804020ull, 0x0000000000008040ull, 0x0000000000000080ull,
    0x4020100804020100ull, 0x8040201008040201ull, 0x0080402010080402ull, 0x0000804020100804ull,
    0x0000008040201008ull, 0x0000000080402010ull, 0x0000000000804020ull, 0x0000000000008040ull,
    0x2010080402010000ull, 0x4020100804020100ull, 0x8040201008040201ull, 0x0080402010080402ull,
    0x0000804020100804ull, 0x0000008040201008ull, 0x0000000080402010ull, 0x0000000000804020ull,
    0x1008040201000000ull, 0x2010080402010000ull, 0x4020100804020100ull, 0x8040201008040201ull,
    0x0080402010080402ull, 0x0000804020100804ull, 0x0000008040201008ull, 0x0000000080402010ull,
    0x0804020100000000ull, 0x1008040201000000ull, 0x2010080402010000ull, 0x4020100804020100ull,
    0x8040201008040201ull, 0x0080402010080402ull, 0x0000804020100804ull, 0x0000008040201008ull,
    0x0402010000000000ull, 0x0804020100000000ull, 0x1008040201000000ull, 0x2010080402010000ull,
    0x4020100804020100ull, 0x8040201008040201ull, 0x0080402010080402ull, 0x0000804020100804ull,
    0x0201000000000000ull, 0x0402010000000000ull, 0x0804020100000000ull, 0x1008040201000000ull,
    0x2010080402010000ull, 0x4020100804020100ull, 0x8040201008040201ull, 0x0080402010080402ull,
    0x0100000000000000ull, 0x0201000000000000ull, 0x0402010000000000ull, 0x0804020100000000ull,
    0x1008040201000000ull, 0x2010080402010000ull, 0x4020100804020100ull, 0x8040201008040201ull,
};

constexpr uint64_t Antis64[64] = {
    0x0000000000000001ull, 0x0000000000000102ull, 0x0000000000010204ull, 0x0000000001020408ull,
    0x0000000102040810ull, 0x0000010204081020ull, 0x0001020408102040ull, 0x0102040810204080ull,
    0x0000000000000102ull, 0x0000000000010204ull, 0x0000000001020408ull, 0x0000000102040810ull,
    0x0000010204081020ull, 0x0001020408102040ull, 0x0102040810204080ull, 0x0204081020408000ull,
    0x0000000000010204ull, 0x0000000001020408ull, 0x0000000102040810ull, 0x0000010204081020ull,
    0x0001020408102040ull, 0x0102040810204080ull, 0x0204081020408000ull, 0x0408102040800000ull,
    0x0000000001020408ull, 0x0000000102040810ull, 0x0000010204081020ull, 0x0001020408102040ull,
    0x0102040810204080ull, 0x0204081020408000ull, 0x0408102040800000ull, 0x0810204080000000ull,
    0x0000000102040810ull, 0x0000010204081020ull, 0x0001020408102040ull, 0x0102040810204080ull,
    0x0204081020408000ull, 0x0408102040800000ull, 0x0810204080000000ull, 0x1020408000000000ull,
    0x0000010204081020ull, 0x0001020408102040ull, 0x0102040810204080ull, 0x0204081020408000ull,
    0x0408102040800000ull, 0x0810204080000000ull, 0x1020408000000000ull, 0x2040800000000000ull,
    0x0001020408102040ull, 0x0102040810204080ull, 0x0204081020408000ull, 0x0408102040800000ull,
    0x0810204080000000ull, 0x1020408000000000ull, 0x2040800000000000ull, 0x4080000000000000ull,
    0x0102040810204080ull, 0x0204081020408000ull, 0x0408102040800000ull, 0x0810204080000000ull,
    0x1020408000000000ull, 0x2040800000000000ull, 0x4080000000000000ull, 0x8000000000000000ull
};

constexpr u64 bit(int sq)                    { return 1ull << sq; }
constexpr u64 bit(int sq1, int sq2)          { return bit(sq1) | bit(sq2); }
constexpr u64 bit(int sq1, int sq2, int sq3) { return bit(sq1, sq2) | bit(sq3); }

constexpr int  count (u64 bb)        { return std::popcount(bb); }
constexpr int  clz   (u64 bb)        { return std::countl_zero(bb); }
constexpr int  ctz   (u64 bb)        { return std::countr_zero(bb); }
constexpr int  lsb   (u64 bb)        { return      ctz(bb); }
constexpr int  msb   (u64 bb)        { return 63 - clz(bb); }
constexpr bool single(u64 bb)        { return bb && (bb & (bb - 1)) == 0; }
constexpr bool multi (u64 bb)        { return bb && (bb & (bb - 1)) != 0; }
constexpr bool test  (u64 bb, int i) { return bb &  bit(i); }
constexpr u64  set   (u64 bb, int i) { return bb |  bit(i); }
constexpr u64  reset (u64 bb, int i) { return bb & ~bit(i); }
constexpr u64  flip  (u64 bb, int i) { return bb ^  bit(i); }

constexpr u64 bswap(u64 bb)
{
    bb = ((bb >>  8) & 0x00ff00ff00ff00ffull) | ((bb <<  8) & 0xff00ff00ff00ff00ull);
    bb = ((bb >> 16) & 0x0000ffff0000ffffull) | ((bb << 16) & 0xffff0000ffff0000ull);
    bb = ((bb >> 32) & 0x00000000ffffffffull) | ((bb << 32) & 0xffffffff00000000ull);

    return bb;
}

constexpr int pop(u64& bb)
{
    int sq = lsb(bb);

    bb &= bb - 1;

    return sq;
}

u64 PawnAttacks      (Side sd, u64 pawns);
u64 PawnAttacksSpan  (Side sd, u64 pawns);
u64 PawnAttacksDouble(Side sd, u64 pawns);
u64 PawnSingles      (Side sd, u64 pawns, u64 empty);
u64 PawnDoubles      (Side sd, u64 pawns, u64 empty);
u64 KnightAttacks    (u64 knights);

template <int N>
constexpr u64 Shift(u64 bb)
{
    return N > 0 ? bb << N : bb >> -N;
}

std::string dump(u64 occ);

// This bitboard sliding-move-generation scheme is ripped
// from https://github.com/lithander/Leorik by Thomas Jahn

namespace Leorik {
    static constexpr u64 hmask(u64 bb)
    {
        return 0x7fffffffffffffffull >> clz(bb | 1);
    }

    static constexpr u64 lmask(u64 bb)
    {
        return bb ^ (bb - 1);
    }

    static constexpr u64 gline(u64 line, u64 blocker, u64 below)
    {
        return (lmask(blocker & ~below) ^ hmask(blocker & below)) & line;
    }

    static constexpr u64 glines(u64 line1, u64 line2, u64 blocker, u64 below)
    {
        return gline(line1, blocker & line1, below) | gline(line2, blocker & line2, below);
    }

    static constexpr u64 Bishop(int sq, u64 occ)
    {
        u64 piece   = 1ull << sq;
        u64 blocker = occ & ~piece;
        u64 below   = piece - 1;

        return glines(Diags64[sq], Antis64[sq], blocker, below) & ~piece;
    }

    static constexpr u64 Rook(int sq, u64 occ)
    {
        u64 piece   = 1ull << sq;
        u64 blocker = occ & ~piece;
        u64 below   = piece - 1;

        return glines(Ranks64[sq], Files64[sq], blocker, below) & ~piece;
    }

    static constexpr u64 Queen(int sq, u64 occ)
    {
        return Rook(sq, occ) | Bishop(sq, occ);
    }
}

}

#endif
