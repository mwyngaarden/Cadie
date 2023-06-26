#include <cassert>
#include <cstddef>
#include <cstdint>
#include "misc.h"
#include "piece.h"
using namespace std;


bool piece256_is_ok(u8 piece)
{
    return P256ToP12[piece] != PieceCount12;
}

int to_piece12(side_t side, int p6)
{
    assert(side_is_ok(side));
    assert(piece_is_ok(p6));

    return (p6 << 1) | side;
}

u8 to_piece256(side_t side, int p6)
{
    assert(side_is_ok(side));
    assert(piece_is_ok(p6));

    return P12ToP256[to_piece12(side, p6)];
}

u8 char_to_piece256(char c)
{
    switch (c) {
    case 'P': return WP256;
    case 'N': return WN256;
    case 'B': return WB256;
    case 'R': return WR256;
    case 'Q': return WQ256;
    case 'K': return WK256;
    case 'p': return BP256;
    case 'n': return BN256;
    case 'b': return BB256;
    case 'r': return BR256;
    case 'q': return BQ256;
    case 'k': return BK256;
    default:
        assert(false);
        return PieceInvalid256;
    }
}

char piece256_to_char(u8 piece)
{
    assert(P256ToP12[piece] != PieceCount12);

    constexpr char ptoc[12] = { 'P', 'p', 'N', 'n', 'B', 'b', 'R', 'r', 'Q', 'q', 'K', 'k' };

    return ptoc[P256ToP12[piece]];
}
