#include <array>
#include <iomanip>
#include <limits>
#include "attack.h"
#include "eval.h"
#include "gen.h"
#include "list.h"
#include "piece.h"
#include "see.h"
#include "uci.h"

using namespace std;

namespace see {

int max(const Move& m)
{
    assert(!m.is_castle());

    int see = 0;

    if (m.is_capture()) {
        int type = P256ToP6[m.capture_piece()];
        see += see::value[type];
    }

    if (m.is_promo()) {
        int type = m.promo_piece6();
        see += see::value[type] - see::value[Pawn];
    }

    return see;
}

int calc(const Position& pos, const Move& m, int threshold)
{
    assert(m.is_valid());
    assert(!m.is_castle());
    
#if PROFILE >= PROFILE_SOME
    gstats.stests++;
#endif

    if (m.is_castle() || m.is_ep() || m.is_promo())
        return 1;

    int orig = m.orig64();
    int dest = m.dest64();
    
    int v = see::max(m);

    v -= threshold;

    if (v < 0) return 0;

    v -= see::value[P256ToP6[pos.board(orig)]];

    if (v >= 0) return 1;
    
    u64 occ = (pos.occ() ^ bb::bit(orig)) | bb::bit(dest);
    u64 att = attackers_to(pos, occ, dest);
    
    u64 bishops = pos.bb(Bishop) | pos.bb(Queen);
    u64 rooks = pos.bb(Rook) | pos.bb(Queen);
    
    side_t side = !pos.side();

    while (true) {

        att &= occ;

        u64 matt = att & pos.occ(side);
    
        if (matt == 0) break;

        int type;
        for (type = Pawn; type < King; type++)
            if (matt & pos.bb(side, type))
                break;

        side = !side;

        v = -v - 1 - see::value[type];

        if (v >= 0) {
            if (type == King && (att & pos.occ(side)))
                side = !side;

            break;
        }

        occ ^= bb::bit(bb::lsb(matt & pos.bb(side^1, type)));

        if (type == Pawn || type == Bishop || type == Queen)
            att |= bb::Leorik::Bishop(dest, occ) & bishops;
        
        if (type == Rook || type == Queen)
            att |= bb::Leorik::Rook(dest, occ) & rooks;
    }

    return side != pos.side();
}

}
