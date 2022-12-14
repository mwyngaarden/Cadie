#ifndef PSQT_H
#define PSQT_H

#include <cstdint>

const int16_t psqt_pawn_mg[128] = {
      0,  0, 0,  0,  0, 0,  0,   0, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0,  5,  5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 10, 10, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 20, 20, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 15, 15, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0,  5,  5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0,  5,  5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0, 0,  0,  0, 0,  0,   0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const int16_t psqt_pawn_eg[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_knight_mg[128] = {
    -65, -35, -30, -35, -35, -30, -35, -65, 0, 0, 0, 0, 0, 0, 0, 0,
    -25, -25, -20,  -5,  -5, -20, -25, -25, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,  -5,   0,   5,   5,   0,  -5, -30, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,  10,   0,   5,   5,   0,  10, -10, 0, 0, 0, 0, 0, 0, 0, 0,
      5,   0,  15,  10,  10,  15,   0,   5, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,  20,  70,  65,  65,  70,  20, -10, 0, 0, 0, 0, 0, 0, 0, 0,
     -5,   5,  60,  45,  45,  60,   5,  -5, 0, 0, 0, 0, 0, 0, 0, 0,
    -20,   0,   0,   0,   0,   0,   0, -20, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_knight_eg[128] = {
    -40, -30, -20, -15, -15, -20, -30, -40, 0, 0, 0, 0, 0, 0, 0, 0, 
    -30, -20, -10,  -5,  -5, -10, -20, -30, 0, 0, 0, 0, 0, 0, 0, 0,
    -20, -10,   0,   5,   5,   0, -10, -20, 0, 0, 0, 0, 0, 0, 0, 0,
    -15,  -5,   5,  10,  10,   5,  -5, -15, 0, 0, 0, 0, 0, 0, 0, 0,
    -15,  -5,   5,  10,  10,   5,  -5, -15, 0, 0, 0, 0, 0, 0, 0, 0,
    -20, -10,   0,   5,   5,   0, -10, -20, 0, 0, 0, 0, 0, 0, 0, 0,
    -30, -20, -10,  -5,  -5, -10, -20, -30, 0, 0, 0, 0, 0, 0, 0, 0,
    -40, -30, -20, -15, -15, -20, -30, -40, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_bishop_mg[128] = {
    -20, -20,-15,-15,-15,-15, -20, -20, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,   0,  0,  0,  0,  0,   0, -10, 0, 0, 0, 0, 0, 0, 0, 0, 
     -5,   0,  5,  0,  0,  5,   0,  -5, 0, 0, 0, 0, 0, 0, 0, 0,
     -5,   0,  0, 10, 10,  0,   0,  -5, 0, 0, 0, 0, 0, 0, 0, 0,
     -5,   0,  0, 10, 10,  0,   0,  -5, 0, 0, 0, 0, 0, 0, 0, 0,
     -5,   0,  5,  0,  0,  5,   0,  -5, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,   0,  0,  0,  0,  0,   0, -10, 0, 0, 0, 0, 0, 0, 0, 0, 
    -10, -10, -5, -5, -5, -5, -10, -10, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_bishop_eg[128] = {
    -25, -20, -15, -15, -15, -15, -20, -25, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,  -5,   0,   0,   0,   0,  -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  10,  10,  15,  15,  10,  10,   0, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  15,  20,  25,  25,  20,  15,  10, 0, 0, 0, 0, 0, 0, 0, 0,
     15,  20,  25,  25,  25,  25,  20,  15, 0, 0, 0, 0, 0, 0, 0, 0,
     15,  20,  25,  25,  25,  25,  20,  15, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  15,  20,  20,  20,  20,  15,  10, 0, 0, 0, 0, 0, 0, 0, 0,
      0,   5,  10,  10,  10,  10,   5,   0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_rook_mg[128] = {
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -10, -5, 0, 5, 5, 0, -5, -10, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_rook_eg[128] = {
    -20, -20, -20, -20, -20, -20, -20, -20, 0, 0, 0, 0, 0, 0, 0, 0, 
    -15, -15, -15, -15, -15, -15, -15, -15, 0, 0, 0, 0, 0, 0, 0, 0, 
    -10, -10,  -5,  -5,  -5,  -5, -10, -10, 0, 0, 0, 0, 0, 0, 0, 0, 
      0,   0,   5,   5,   5,   5,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  10,  15,  15,  15,  15,  10,  10, 0, 0, 0, 0, 0, 0, 0, 0, 
     15,  15,  20,  20,  20,  20,  15,  15, 0, 0, 0, 0, 0, 0, 0, 0, 
     15,  20,  20,  20,  20,  20,  20,  15, 0, 0, 0, 0, 0, 0, 0, 0, 
     10,  10,  15,  15,  15,  15,  10,  10, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_queen_mg[128] = {
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_queen_eg[128] = {
    -60, -55, -50, -50, -50, -50, -55, -60, 0, 0, 0, 0, 0, 0, 0, 0,
    -30, -25, -20, -20, -20, -20, -25, -30, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,  -5,   0,   5,   5,   0,  -5, -10, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  15,  15,  20,  20,  15,  15,  10, 0, 0, 0, 0, 0, 0, 0, 0,
     20,  25,  30,  30,  30,  30,  25,  20, 0, 0, 0, 0, 0, 0, 0, 0,
     25,  30,  30,  35,  35,  30,  30,  25, 0, 0, 0, 0, 0, 0, 0, 0,
     20,  25,  30,  30,  30,  30,  25,  20, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  15,  20,  25,  25,  20,  15,  10, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_king_mg[128] = {
     40,  50,  30,  10,  10,  30,  50,  40, 0, 0, 0, 0, 0, 0, 0, 0,
     30,  40,  20,   0,   0,  20,  40,  30, 0, 0, 0, 0, 0, 0, 0, 0,
     10,  20,   0, -20, -20,   0,  20,  10, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  10, -10, -30, -30, -10,  10,   0, 0, 0, 0, 0, 0, 0, 0, 0,
    -10,   0, -20, -40, -40, -20,   0, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    -20, -10, -30, -50, -50, -30, -10, -20, 0, 0, 0, 0, 0, 0, 0, 0,
    -30, -20, -40, -60, -60, -40, -20, -30, 0, 0, 0, 0, 0, 0, 0, 0,
    -40, -30, -50, -70, -70, -50, -30, -40, 0, 0, 0, 0, 0, 0, 0, 0
};

const int16_t psqt_king_eg[128] = {
    -70, -50, -35, -25, -25, -35, -50, -70, 0, 0, 0, 0, 0, 0, 0, 0,
    -50, -25, -10,   0,   0, -10, -25, -50, 0, 0, 0, 0, 0, 0, 0, 0,
    -35, -10,   0,  10,  10,   0, -10, -35, 0, 0, 0, 0, 0, 0, 0, 0,
    -25,   0,  10,  25,  25,  10,   0, -25, 0, 0, 0, 0, 0, 0, 0, 0,
    -25,   0,  10,  25,  25,  10,   0, -25, 0, 0, 0, 0, 0, 0, 0, 0,
    -35, -10,   0,  10,  10,   0, -10, -35, 0, 0, 0, 0, 0, 0, 0, 0,
    -50, -25, -10,   0,   0, -10, -25, -50, 0, 0, 0, 0, 0, 0, 0, 0,
    -70, -50, -35, -25, -25, -35, -50, -70, 0, 0, 0, 0, 0, 0, 0, 0
};

#endif
