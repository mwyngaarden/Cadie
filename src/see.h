#ifndef SEE_H
#define SEE_H

#include <limits>
#include <string>
#include "move.h"
#include "piece.h"
#include "pos.h"

namespace see {

constexpr int value[6] = { 100, 325, 325, 500, 1000, 0 };

int max(const Move& m);
int calc(const Position& pos, const Move& m, int threshold = 0);

}

#endif
