#ifndef PIECE_H
#define PIECE_H

#include <array>
#include <utility>
#include <cstdint>
#include "misc.h"

constexpr int SEEValue[6 + 3] = { 100, 325, 325, 500, 1000, 0, 0, 0, 0 };

enum Side : i8 { White, Black, None };
enum Piece : i8 { Pawn, Knight, Bishop, Rook, Queen, King, None6 };
enum Piece12 : i8 { WP12, BP12, WN12, BN12, WB12, BB12, WR12, BR12, WQ12, BQ12, WK12, BK12, None12 = 16 };

constexpr Side operator!(Side sd) { return sd == White ? Black : White; }
constexpr Side operator%(Piece12 pt12, int n) { return Side(int(pt12) % n); }
constexpr Piece operator/(Piece12 pt12, int n) { return Piece(int(pt12) / n); }
constexpr Piece12 operator+(Piece12 pt12, Side sd) { return Piece12(int(pt12) + int(sd)); }
constexpr Piece12 operator-(Piece12 pt12, Side sd) { return Piece12(int(pt12) - int(sd)); }

char piece12_to_char(Piece12 pt);
Piece12 char_to_piece12(char c);

#endif
