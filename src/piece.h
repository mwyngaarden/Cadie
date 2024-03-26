#ifndef PIECE_H
#define PIECE_H

#include <array>
#include <utility>
#include <cstdint>
#include "misc.h"

enum class PieceAttr : int { Color2, Type6, Type12 };

typedef u8 side_t;

constexpr side_t White          =  0;
constexpr side_t Black          =  1;

constexpr int Pawn              =  0;
constexpr int Knight            =  1;
constexpr int Bishop            =  2;
constexpr int Rook              =  3;
constexpr int Queen             =  4;
constexpr int King              =  5;

constexpr int WhitePawn12       =  0;
constexpr int BlackPawn12       =  1;
constexpr int WhiteKnight12     =  2;
constexpr int BlackKnight12     =  3;
constexpr int WhiteBishop12     =  4;
constexpr int BlackBishop12     =  5;
constexpr int WhiteRook12       =  6;
constexpr int BlackRook12       =  7;
constexpr int WhiteQueen12      =  8;
constexpr int BlackQueen12      =  9;
constexpr int WhiteKing12       = 10;
constexpr int BlackKing12       = 11;

constexpr u8 PieceNone256       = 0;
constexpr u8 WhiteFlag256       = 1 << 0;
constexpr u8 BlackFlag256       = 1 << 1;
constexpr u8 ColorFlags256      = WhiteFlag256 | BlackFlag256;

constexpr u8 WhitePawnFlag256   = 1 << 2;
constexpr u8 BlackPawnFlag256   = 1 << 3;
constexpr u8 PawnFlags256       = WhitePawnFlag256 | BlackPawnFlag256;

constexpr u8 KnightFlag256      = 1 << 4;
constexpr u8 BishopFlag256      = 1 << 5;
constexpr u8 RookFlag256        = 1 << 6;
constexpr u8 QueenFlags256      = BishopFlag256 | RookFlag256;
constexpr u8 KingFlag256        = 1 << 7;

constexpr u8 PieceFlags256      = PawnFlags256 | KnightFlag256 | QueenFlags256 | KingFlag256;

constexpr u8 WhitePawn256       = WhiteFlag256 | WhitePawnFlag256;
constexpr u8 WhiteKnight256     = WhiteFlag256 | KnightFlag256;
constexpr u8 WhiteBishop256     = WhiteFlag256 | BishopFlag256;
constexpr u8 WhiteRook256       = WhiteFlag256 | RookFlag256;
constexpr u8 WhiteQueen256      = WhiteFlag256 | QueenFlags256;
constexpr u8 WhiteKing256       = WhiteFlag256 | KingFlag256;
constexpr u8 BlackPawn256       = BlackFlag256 | BlackPawnFlag256;
constexpr u8 BlackKnight256     = BlackFlag256 | KnightFlag256;
constexpr u8 BlackBishop256     = BlackFlag256 | BishopFlag256;
constexpr u8 BlackRook256       = BlackFlag256 | RookFlag256;
constexpr u8 BlackQueen256      = BlackFlag256 | QueenFlags256;
constexpr u8 BlackKing256       = BlackFlag256 | KingFlag256;
constexpr u8 PieceInvalid256    = ~(ColorFlags256 | PawnFlags256);

constexpr u8 WP256 = WhitePawn256;
constexpr u8 WN256 = WhiteKnight256;
constexpr u8 WB256 = WhiteBishop256;
constexpr u8 WR256 = WhiteRook256;
constexpr u8 WQ256 = WhiteQueen256;
constexpr u8 WK256 = WhiteKing256;
constexpr u8 BP256 = BlackPawn256;
constexpr u8 BN256 = BlackKnight256;
constexpr u8 BB256 = BlackBishop256;
constexpr u8 BR256 = BlackRook256;
constexpr u8 BQ256 = BlackQueen256;
constexpr u8 BK256 = BlackKing256;

constexpr u8 WP12 = WhitePawn12;
constexpr u8 WN12 = WhiteKnight12;
constexpr u8 WB12 = WhiteBishop12;
constexpr u8 WR12 = WhiteRook12;
constexpr u8 WQ12 = WhiteQueen12;
constexpr u8 WK12 = WhiteKing12;
constexpr u8 BP12 = BlackPawn12;
constexpr u8 BN12 = BlackKnight12;
constexpr u8 BB12 = BlackBishop12;
constexpr u8 BR12 = BlackRook12;
constexpr u8 BQ12 = BlackQueen12;
constexpr u8 BK12 = BlackKing12;

constexpr bool is_white(u8 piece) { return (piece & WhiteFlag256) != PieceNone256; }
constexpr bool is_black(u8 piece) { return (piece & BlackFlag256) != PieceNone256; }
constexpr bool is_pawn(u8 piece) { return (piece & PawnFlags256) != PieceNone256; }
constexpr bool is_knight(u8 piece) { return (piece & KnightFlag256) != PieceNone256; }
constexpr bool is_bishop(u8 piece) { return (piece & QueenFlags256) == BishopFlag256; }
constexpr bool is_rook(u8 piece) { return (piece & QueenFlags256) == RookFlag256; }
constexpr bool is_queen(u8 piece) { return (piece & QueenFlags256) == QueenFlags256; }
constexpr bool is_king(u8 piece) { return (piece & KingFlag256) != PieceNone256; }
constexpr bool is_slider(u8 piece) { return (piece & QueenFlags256) != PieceNone256; }

constexpr u8 flip_pawn(u8 piece) { return piece ^ (ColorFlags256 | PawnFlags256); }
constexpr u8 make_pawn(side_t side) { return WhitePawn256 << side; }
constexpr u8 make_flag(side_t side) { return side + 1; }

constexpr bool side_is_ok(side_t side) { return side == White || side == Black; }
constexpr bool piece_is_ok(int type) { return type >= Pawn && type <= King; }
constexpr bool piece12_is_ok(int p12) { return p12 >= WhitePawn12 && p12 <= BlackKing12; }

bool piece256_is_ok(u8 piece);
char piece256_to_char(u8 piece);
u8 char_to_piece256(char c);
u8 to_piece256(side_t side, int type);
int to_piece12(side_t side, int type);

constexpr auto P256ToP6
{
    []() constexpr
    {
        std::array<int, 256> result;

        result.fill(6);
    
        result[WP256] = Pawn;
        result[WN256] = Knight;
        result[WB256] = Bishop;
        result[WR256] = Rook;
        result[WQ256] = Queen;
        result[WK256] = King;
        result[BP256] = Pawn;
        result[BN256] = Knight;
        result[BB256] = Bishop;
        result[BR256] = Rook;
        result[BQ256] = Queen;
        result[BK256] = King;
        
        return result;
    }()
};

constexpr auto P256ToP12
{
    []() constexpr
    {
        std::array<int, 256> result;

        result.fill(12);

        result[WP256] = WP12;
        result[WN256] = WN12;
        result[WB256] = WB12;
        result[WR256] = WR12;
        result[WQ256] = WQ12;
        result[WK256] = WK12;
        result[BP256] = BP12;
        result[BN256] = BN12;
        result[BB256] = BB12;
        result[BR256] = BR12;
        result[BQ256] = BQ12;
        result[BK256] = BK12;
        
        return result;
    }()
};

constexpr auto P12ToP256
{
    []() constexpr
    {
        std::array<u8, 12> result{};

        result[WP12] = WP256;
        result[WN12] = WN256;
        result[WB12] = WB256;
        result[WR12] = WR256;
        result[WQ12] = WQ256;
        result[WK12] = WK256;
        result[BP12] = BP256;
        result[BN12] = BN256;
        result[BB12] = BB256;
        result[BR12] = BR256;
        result[BQ12] = BQ256;
        result[BK12] = BK256;
        
        return result;
    }()
};

#endif
