#ifndef GEN_H
#define GEN_H

#include "misc.h"
#include "move.h"
#include "piece.h"
#include "pos.h"

enum class GenMode : int { Pseudo, Legal, Tactical, Count };
enum class GenState : int { Normal, Stalemate, Checkmate };

constexpr int IncrsN[8] = { -33, -31, -18, -14, 14, 18, 31, 33 };
constexpr int IncrsB[4] = { -17, -15,  15,  17 };
constexpr int IncrsR[4] = { -16,  -1,   1,  16 };
constexpr int IncrsQ[8] = { -17, -16, -15,  -1,  1, 15, 16, 17 };

std::size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode);
u64 gen_pins(const Position& pos);

GenState gen_state(const Position& pos);

int  delta_incr   (int orig, int dest);
bool pseudo_attack(int orig, int dest, u8 piece);

constexpr int DeltaCount = 240;
constexpr int DeltaOffset = 120;

constexpr auto DeltaIncr
{
    []() constexpr
    {
        std::array<i8, DeltaCount> result{};

        for (auto incr : IncrsN)
            result[DeltaOffset + incr] = incr;

        for (auto incr : IncrsB)
            for (int i = 1; i <= 7; i++)
                result[DeltaOffset + incr * i] = incr;

        for (auto incr : IncrsR)
            for (int i = 1; i <= 7; i++)
                result[DeltaOffset + incr * i] = incr;
        
        return result;
    }()
};

constexpr auto DeltaType
{
    []() constexpr
    {
        std::array<u8, DeltaCount> result{};

        result[DeltaOffset + 16 - 1] |= WhitePawnFlag256;
        result[DeltaOffset + 16 + 1] |= WhitePawnFlag256;
        result[DeltaOffset - 16 - 1] |= BlackPawnFlag256;
        result[DeltaOffset - 16 + 1] |= BlackPawnFlag256;

        for (auto incr : IncrsN)
            result[DeltaOffset + incr] |= KnightFlag256;

        for (auto incr : IncrsB)
            for (int i = 1; i <= 7; i++)
                result[DeltaOffset + incr * i] |= BishopFlag256;

        for (auto incr : IncrsR)
            for (int i = 1; i <= 7; i++)
                result[DeltaOffset + incr * i] |= RookFlag256;

        for (auto incr : IncrsQ)
            result[DeltaOffset + incr] |= KingFlag256;
        
        return result;
    }()
};

constexpr auto CastleMasks
{
    []() constexpr
    {
        std::array<u8, 64> result;

        result.fill(~0);

        result[square::A1] = ~Position::WhiteCastleQFlag;
        result[square::E1] = ~(Position::WhiteCastleQFlag | Position::WhiteCastleKFlag);
        result[square::H1] = ~Position::WhiteCastleKFlag;
        result[square::A8] = ~Position::BlackCastleQFlag;
        result[square::E8] = ~(Position::BlackCastleQFlag | Position::BlackCastleKFlag);
        result[square::H8] = ~Position::BlackCastleKFlag;
        
        return result;
    }()
};

#endif
