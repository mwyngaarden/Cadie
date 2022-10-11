#ifndef SEE_H
#define SSE_H

#include <string>
#include "move.h"
#include "pos.h"

int see_move(const Position& pos, Move m);

struct SeeInfo {
    std::string fen;
    std::string move;
    int value;
};

void see_validate();

#endif
