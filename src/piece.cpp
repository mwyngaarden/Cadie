#include <cassert>
#include <cstddef>
#include <cstdint>
#include "piece.h"
#include "types.h"
using namespace std;

u8 P12ToP256[12];
int P256ToP12[256];
int P256ToP6[256];

void piece_init()
{
    for (int i = 0; i < 256; i++)
        P256ToP12[i] = PieceNone12;

    P12ToP256[WP12] = WP256;
    P12ToP256[WN12] = WN256;
    P12ToP256[WB12] = WB256;
    P12ToP256[WR12] = WR256;
    P12ToP256[WQ12] = WQ256;
    P12ToP256[WK12] = WK256;
    P12ToP256[BP12] = BP256;
    P12ToP256[BN12] = BN256;
    P12ToP256[BB12] = BB256;
    P12ToP256[BR12] = BR256;
    P12ToP256[BQ12] = BQ256;
    P12ToP256[BK12] = BK256;

    P256ToP12[WP256] = WP12;
    P256ToP12[WN256] = WN12;
    P256ToP12[WB256] = WB12;
    P256ToP12[WR256] = WR12;
    P256ToP12[WQ256] = WQ12;
    P256ToP12[WK256] = WK12;
    P256ToP12[BP256] = BP12;
    P256ToP12[BN256] = BN12;
    P256ToP12[BB256] = BB12;
    P256ToP12[BR256] = BR12;
    P256ToP12[BQ256] = BQ12;
    P256ToP12[BK256] = BK12;
    
    P256ToP6[WP256] = Pawn;
    P256ToP6[WN256] = Knight;
    P256ToP6[WB256] = Bishop;
    P256ToP6[WR256] = Rook;
    P256ToP6[WQ256] = Queen;
    P256ToP6[WK256] = King;
    P256ToP6[BP256] = Pawn;
    P256ToP6[BN256] = Knight;
    P256ToP6[BB256] = Bishop;
    P256ToP6[BR256] = Rook;
    P256ToP6[BQ256] = Queen;
    P256ToP6[BK256] = King;
}

bool piece256_is_ok(u8 piece)
{
    return P256ToP12[piece] != PieceNone12;
}

int to_piece12(side_t side, int ptype)
{
    assert(side_is_ok(side));
    assert(piece_is_ok(ptype));

    return (ptype << 1) | side;
}

u8 to_piece256(side_t side, int ptype)
{
    assert(side_is_ok(side));
    assert(piece_is_ok(ptype));

    return P12ToP256[to_piece12(side, ptype)];
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
    assert(P256ToP12[piece] != PieceNone12);

    constexpr char ptoc[12] = { 'P', 'p', 'N', 'n', 'B', 'b', 'R', 'r', 'Q', 'q', 'K', 'k' };

    return ptoc[P256ToP12[piece]];
}
