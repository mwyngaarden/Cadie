#include <cstddef>
#include <cstdint>
#include "misc.h"
#include "piece.h"
using namespace std;


Piece12 char_to_piece12(char c)
{
    switch (c) {
    case 'P': return WP12;
    case 'N': return WN12;
    case 'B': return WB12;
    case 'R': return WR12;
    case 'Q': return WQ12;
    case 'K': return WK12;
    case 'p': return BP12;
    case 'n': return BN12;
    case 'b': return BB12;
    case 'r': return BR12;
    case 'q': return BQ12;
    case 'k': return BK12;
    default:
        return None12;
    }
}

char piece12_to_char(Piece12 pt12)
{
    constexpr char ptoc[12] = { 'P', 'p', 'N', 'n', 'B', 'b', 'R', 'r', 'Q', 'q', 'K', 'k' };

    return ptoc[pt12];
}
