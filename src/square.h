#ifndef SQUARE_H
#define SQUARE_H

#include <string>
#include <cassert>
#include <cstdint>
#include "piece.h"

enum {
    A1 =   0, B1, C1, D1, E1, F1, G1, H1,
    A2 =  16, B2, C2, D2, E2, F2, G2, H2,
    A3 =  32, B3, C3, D3, E3, F3, G3, H3,
    A4 =  48, B4, C4, D4, E4, F4, G4, H4,
    A5 =  64, B5, C5, D5, E5, F5, G5, H5,
    A6 =  80, B6, C6, D6, E6, F6, G6, H6,
    A7 =  96, B7, C7, D7, E7, F7, G7, H7,
    A8 = 112, B8, C8, D8, E8, F8, G8, H8,
    SquareCount = 128, SquareNone = 255
};

enum : int { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH };
enum : int { Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 };

constexpr bool sq88_is_ok(int sq)
{
    return (sq & ~0x77) == 0;
}

constexpr bool sq64_is_ok(int sq)
{
    return sq >= 0 && sq < 64;
}

constexpr bool rank_is_ok(int rank)
{
    return rank >= Rank1 && rank <= Rank8;
}

constexpr bool file_is_ok(int file)
{
    return file >= FileA && file <= FileH;
}

constexpr int pawn_incr(side_t side)
{
    return 16 - 32 * side;
}

constexpr int sq88_file(int sq)
{
    return sq & 7;
}

constexpr int sq88_rank(int sq)
{
    return sq >> 4;
}

constexpr int sq88_rank(int sq, side_t side)
{
    int rank = sq88_rank(sq);

    return rank ^ (7 * side);
}

constexpr int sq64_color(int sq)
{
   return (0xaa55aa55aa55aa55ull >> sq) & 1;
}

constexpr int to_sq64(int sq)
{
    return (sq + (sq & 7)) >> 1;
}

constexpr int to_sq88(int sq)
{
    return sq + (sq & ~7);
}

constexpr int to_sq64(int file, int rank)
{
    return (rank << 3) | file;
}

constexpr int to_sq88(int file, int rank)
{
    return (rank << 4) | file;
}

namespace square {

    int san_to_sq(std::string s);

    std::string sq_to_san(int sq);

    int dist(int sq1, int sq2);
    int dist_edge(int sq);

    enum {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
        SquareCount = 64, SquareNone = 65
    };

    constexpr int rank(int sq)
    {
        return sq >> 3;
    }

    constexpr int rank(int sq, side_t side)
    {
        return rank(sq) ^ (7 * side);
    }
    
    constexpr int file(int sq)
    {
        return sq & 7;
    }

    constexpr int color(int sq)
    {
       return (0xaa55aa55aa55aa55ull >> sq) & 1;
    }

    constexpr bool corner(int sq)
    {
        return sq == A1 || sq == A8 || sq == H1 || sq == H8;
    }

    constexpr int reflect(int sq)
    {
        return sq ^ 0x38;
    }

    constexpr int ep_dual(int sq)
    {
        return sq ^ 8;
    }

    constexpr int incr(side_t side)
    {
        return 8 - 16 * side;
    }

}

#endif
