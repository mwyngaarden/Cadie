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

u64 attackers_to(const Position& pos, u64 occ, int sq)
{
    return (PawnAttacks[White][sq] & pos.bb(Black, Pawn))
         | (PawnAttacks[Black][sq] & pos.bb(White, Pawn))
         | (bb::Leorik::Knight(sq, pos.bb(Knight)))
         | (bb::Leorik::Bishop(sq, occ) & (pos.bb(Bishop) | pos.bb(Queen)))
         | (bb::Leorik::Rook(sq, occ) & (pos.bb(Rook) | pos.bb(Queen)))
         | (bb::Leorik::King(sq, pos.bb(King)));
}

int max(const Move& m)
{
    assert(!m.is_castle());

    int see = 0;

    if (m.is_capture()) {
        int type = P256ToP6[m.captured()];
        see += see::value[type];
    }

    if (m.is_promo()) {
        int type = m.promo6();
        see += see::value[type] - see::value[Pawn];
    }

    return see;
}

int calc(const Position& pos, const Move& m, int threshold)
{
    assert(m.is_valid());
    
#if PROFILE >= PROFILE_SOME
    gstats.stests++;
#endif

    if (m.is_under_promo()) [[unlikely]]
        return 0;

    if (m.is_ep() || m.is_queen_promo() || m.orig() == pos.king()) [[unlikely]]
        return 1;

    int orig = m.orig();
    int dest = m.dest();
    
    int v = see::max(m);

    v -= threshold;

    if (v < 0) return 0;

    v -= see::value[pos.board<6>(orig)];

    if (v >= 0) return 1;
    
    u64 occ = (pos.occ() ^ bb::bit(orig)) | bb::bit(dest);
    u64 att = attackers_to(pos, occ, dest);
    
    u64 bishops = pos.bb(Bishop) | pos.bb(Queen);
    u64 rooks = pos.bb(Rook) | pos.bb(Queen);
    
    side_t side = !pos.side();

    while (true) {

        att &= occ;

        u64 matt = att & pos.bb(side);
    
        if (matt == 0) break;

        int type;

        for (type = Pawn; type < King; type++)
            if (matt & pos.bb(side, type))
                break;

        side = !side;

        v = -v - 1 - see::value[type];

        if (v >= 0) {
            if (type == King && (att & pos.bb(side)))
                side = !side;

            break;
        }

        occ ^= bb::bit(bb::lsb(matt & pos.bb(side_t(!side), type)));

        if (type == Pawn || type == Bishop || type == Queen)
            att |= bb::Leorik::Bishop(dest, occ) & bishops;
        
        if (type == Rook || type == Queen)
            att |= bb::Leorik::Rook(dest, occ) & rooks;
    }

    return side != pos.side();
}

}
