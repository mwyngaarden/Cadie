#ifndef SQUARE_H
#define SQUARE_H

#include <string>
#include <cassert>
#include <cstdint>
#include "piece.h"


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
        None = 64
    };

    constexpr int rank(int sq)
    {
        return sq >> 3;
    }

    constexpr int rank(int sq, side_t side)
    {
        return rank(sq) ^ (7 * side);
    }

    constexpr bool rank_eq(int sq1, int sq2)
    {
        return ((sq1 ^ sq2) & 56) == 0;
    }
    
    constexpr int file(int sq)
    {
        return sq & 7;
    }
    
    constexpr bool file_eq(int sq1, int sq2)
    {
        return ((sq1 ^ sq2) & 7) == 0;
    }

    constexpr int diag(int sq)
    {
        return 7 + (sq >> 3) - (sq & 7);
    }

    constexpr bool diag_eq(int sq1, int sq2)
    {
        return diag(sq1) == diag(sq2);
    }
    
    constexpr int anti(int sq)
    {
        return (sq >> 3) + (sq & 7);
    }

    constexpr bool anti_eq(int sq1, int sq2)
    {
        return anti(sq1) == anti(sq2);
    }

    constexpr int color(int sq)
    {
       return (0xaa55aa55aa55aa55ull >> sq) & 1;
    }

    constexpr int ep_dual(int sq)
    {
        return sq ^ 8;
    }

    constexpr int incr(side_t side)
    {
        return 8 - 16 * side;
    }

    constexpr int pawn_promo(side_t side, int sq)
    {
        return ((side - 1) & 56) + (sq & 7);
    }

    constexpr int relative(side_t side, int sq)
    {
        return sq ^ (side * 56);
    }

    constexpr int make(int file, int rank)
    {
        return (rank << 3) | file;
    }
}

#endif
