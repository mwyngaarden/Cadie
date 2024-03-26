#ifndef BB_H
#define BB_H

#include <bit>
#include <string>
#include "misc.h"
#include "piece.h"

namespace bb {

extern u64 PawnSpan[2][64];
extern u64 PawnSpanAdj[2][64];

void init();

enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};

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
constexpr u64 Light      = 0x55aa55aa55aa55aaull;
constexpr u64 Dark       = ~Light;

constexpr u64 Ranks[8] = { Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 };
constexpr u64 Files[8] = { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH };

constexpr u64 RanksLT[8] = {
    0,
    Rank1,
    Rank1 | Rank2,
    Rank1 | Rank2 | Rank3,
    Rank1 | Rank2 | Rank3 | Rank4,
    Rank1 | Rank2 | Rank3 | Rank4 | Rank5,
    Rank1 | Rank2 | Rank3 | Rank4 | Rank5 | Rank6,
    Rank1 | Rank2 | Rank3 | Rank4 | Rank5 | Rank6 | Rank7,
};

constexpr u64 RanksGT[8] = {
    Rank8 | Rank7 | Rank6 | Rank5 | Rank4 | Rank3 | Rank2,
    Rank8 | Rank7 | Rank6 | Rank5 | Rank4 | Rank3,
    Rank8 | Rank7 | Rank6 | Rank5 | Rank4,
    Rank8 | Rank7 | Rank6 | Rank5,
    Rank8 | Rank7 | Rank6,
    Rank8 | Rank7,
    Rank8,
    0
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

int lsb(u64 bb);
u64 bit(int sq);
u64 bit88(int sq);

int pop     (u64& bb);
bool test   (u64 bb, int i);
bool single (u64 bb);
u64 set     (u64 bb, int i);
u64 reset   (u64 bb, int i);
u64 flip    (u64 bb, int i);

u64 PawnAttacks(u64 pawns, side_t side);
u64 PawnSingles(u64 pawns, u64 empty, side_t side);
u64 PawnDoubles(u64 pawns, u64 empty, side_t side);

std::string dump(u64 occ);

// This bitboard sliding-move-generation scheme is ripped
// from https://github.com/lithander/Leorik by Thomas Jahn
namespace Leorik {
    
static constexpr u64 Horizontal   = 0x00000000000000ffull;
static constexpr u64 Vertical     = 0x0101010101010101ull;
static constexpr u64 Diagonal     = 0x8040201008040201ull;
static constexpr u64 AntiDiagonal = 0x0102040810204080ull;

static constexpr u64 hmask(u64 bb)
{
    return 0x7fffffffffffffffull >> std::countl_zero(bb | 1);
}

static constexpr u64 lmask(u64 bb)
{
    return bb ^ (bb - 1);
}

static constexpr u64 vshift(u64 bb, int ranks)
{
    return ranks > 0 ? bb >> (ranks << 3) : bb << -(ranks << 3);
}

static constexpr u64 line(u64 line1, u64 blocker, u64 below)
{
    return (lmask(blocker & ~below) ^ hmask(blocker & below)) & line1;
}

static constexpr u64 lines(u64 line1, u64 line2, u64 blocker, u64 below)
{
    return line(line1, blocker & line1, below) | line(line2, blocker & line2, below);
}

static constexpr u64 Knight(int sq, u64 targets = Full)
{
    constexpr u64 arr[64] = {
        0x0000000000020400ull, 0x0000000000050800ull, 0x00000000000a1100ull, 0x0000000000142200ull,
        0x0000000000284400ull, 0x0000000000508800ull, 0x0000000000a01000ull, 0x0000000000402000ull,
        0x0000000002040004ull, 0x0000000005080008ull, 0x000000000a110011ull, 0x0000000014220022ull,
        0x0000000028440044ull, 0x0000000050880088ull, 0x00000000a0100010ull, 0x0000000040200020ull,
        0x0000000204000402ull, 0x0000000508000805ull, 0x0000000a1100110aull, 0x0000001422002214ull,
        0x0000002844004428ull, 0x0000005088008850ull, 0x000000a0100010a0ull, 0x0000004020002040ull,
        0x0000020400040200ull, 0x0000050800080500ull, 0x00000a1100110a00ull, 0x0000142200221400ull,
        0x0000284400442800ull, 0x0000508800885000ull, 0x0000a0100010a000ull, 0x0000402000204000ull,
        0x0002040004020000ull, 0x0005080008050000ull, 0x000a1100110a0000ull, 0x0014220022140000ull,
        0x0028440044280000ull, 0x0050880088500000ull, 0x00a0100010a00000ull, 0x0040200020400000ull,
        0x0204000402000000ull, 0x0508000805000000ull, 0x0a1100110a000000ull, 0x1422002214000000ull,
        0x2844004428000000ull, 0x5088008850000000ull, 0xa0100010a0000000ull, 0x4020002040000000ull,
        0x0400040200000000ull, 0x0800080500000000ull, 0x1100110a00000000ull, 0x2200221400000000ull,
        0x4400442800000000ull, 0x8800885000000000ull, 0x100010a000000000ull, 0x2000204000000000ull,
        0x0004020000000000ull, 0x0008050000000000ull, 0x00110a0000000000ull, 0x0022140000000000ull,
        0x0044280000000000ull, 0x0088500000000000ull, 0x0010a00000000000ull, 0x0020400000000000ull
    };

    return arr[sq] & targets;
}

static constexpr u64 Bishop(int sq, u64 occ)
{
    u64 piece   = 1ull << sq;
    u64 blocker = occ & ~piece;
    u64 below   = piece - 1;

    int rank = sq >> 3;
    int file = sq & 7;

    u64 diag  = vshift(    Diagonal,     file - rank);
    u64 adiag = vshift(AntiDiagonal, 7 - file - rank);

    return lines(diag, adiag, blocker, below) & ~piece;
}

static constexpr u64 Rook(int sq, u64 occ)
{
    u64 piece   = 1ull << sq;
    u64 blocker = occ & ~piece;
    u64 below   = piece - 1;
    u64 horiz   = Horizontal << (sq & 56);
    u64 verti   = Vertical << (sq & 7);
    
    return lines(horiz, verti, blocker, below) & ~piece;
}

static constexpr u64 Queen(int sq, u64 occ)
{
    return Rook(sq, occ) | Bishop(sq, occ);
}

static constexpr u64 King(int sq, u64 targets = Full)
{
    constexpr u64 arr[64] = {
        0x0000000000000302ull, 0x0000000000000705ull, 0x0000000000000e0aull, 0x0000000000001c14ull,
        0x0000000000003828ull, 0x0000000000007050ull, 0x000000000000e0a0ull, 0x000000000000c040ull,
        0x0000000000030203ull, 0x0000000000070507ull, 0x00000000000e0a0eull, 0x00000000001c141cull,
        0x0000000000382838ull, 0x0000000000705070ull, 0x0000000000e0a0e0ull, 0x0000000000c040c0ull,
        0x0000000003020300ull, 0x0000000007050700ull, 0x000000000e0a0e00ull, 0x000000001c141c00ull,
        0x0000000038283800ull, 0x0000000070507000ull, 0x00000000e0a0e000ull, 0x00000000c040c000ull,
        0x0000000302030000ull, 0x0000000705070000ull, 0x0000000e0a0e0000ull, 0x0000001c141c0000ull,
        0x0000003828380000ull, 0x0000007050700000ull, 0x000000e0a0e00000ull, 0x000000c040c00000ull,
        0x0000030203000000ull, 0x0000070507000000ull, 0x00000e0a0e000000ull, 0x00001c141c000000ull,
        0x0000382838000000ull, 0x0000705070000000ull, 0x0000e0a0e0000000ull, 0x0000c040c0000000ull,
        0x0003020300000000ull, 0x0007050700000000ull, 0x000e0a0e00000000ull, 0x001c141c00000000ull,
        0x0038283800000000ull, 0x0070507000000000ull, 0x00e0a0e000000000ull, 0x00c040c000000000ull,
        0x0302030000000000ull, 0x0705070000000000ull, 0x0e0a0e0000000000ull, 0x1c141c0000000000ull,
        0x3828380000000000ull, 0x7050700000000000ull, 0xe0a0e00000000000ull, 0xc040c00000000000ull,
        0x0203000000000000ull, 0x0507000000000000ull, 0x0a0e000000000000ull, 0x141c000000000000ull,
        0x2838000000000000ull, 0x5070000000000000ull, 0xa0e0000000000000ull, 0x40c0000000000000ull
    };

    return arr[sq] & targets;
}
}
}

#endif
