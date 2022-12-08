#ifndef SEE_H
#define SEE_H

#include <string>
#include "move.h"
#include "pos.h"

int see_move(const Position& pos, const Move& m);

void see_validate();

#endif
