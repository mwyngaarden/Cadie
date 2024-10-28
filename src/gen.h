#ifndef GEN_H
#define GEN_H

#include "misc.h"
#include "move.h"
#include "piece.h"
#include "pos.h"

enum class GenMode : int { Pseudo, Legal, Tactical, Count };
enum class GenState : int { Normal, Stalemate, Checkmate };

std::size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode);
std::size_t add_checks(MoveList& moves, const Position& pos);

GenState gen_state(const Position& pos);

int delta_incr(int orig, int dest);

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
