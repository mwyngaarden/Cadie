#include <algorithm>
#include <cstdint>
#include "attack.h"
#include "bb.h"
#include "gen.h"
#include "misc.h"
#include "square.h"
using namespace std;

u64 KingZone[64];
u64 PawnAttacks[2][64];
u64 PieceAttacks[6][64];
u64 InBetween[64][64];

u64 attackers_to(const Position& pos, u64 occ, int sq)
{
    return (PawnAttacks[White][sq] & pos.bb(Black, Pawn))
         | (PawnAttacks[Black][sq] & pos.bb(White, Pawn))
         | (PieceAttacks[Knight][sq] & pos.bb(Knight))
         | (bb::Leorik::Bishop(sq, occ) & (pos.bb(Bishop) | pos.bb(Queen)))
         | (bb::Leorik::Rook(sq, occ) & (pos.bb(Rook) | pos.bb(Queen)))
         | (PieceAttacks[King][sq] & pos.bb(King));
}

void attack_init()
{
    int sq;

    for (int i = 0; i < 64; i++) {
        const int orig = to_sq88(i);

        // King zone
        
        int ksq = orig;

        if (ksq == A1) ksq = B2;
        if (ksq == A8) ksq = B7;
        if (ksq == H1) ksq = G2;
        if (ksq == H8) ksq = G7;

        for (auto incr : IncrsQ) {
            sq = ksq + incr;

            if (sq88_is_ok(sq))
                KingZone[i] |= bb::bit88(sq);
        }

        // Pawn attacks
        
        for (side_t side : { White, Black }) {
            const int pincr = pawn_incr(side);

            for (int delta : { -1, 1 }) {
                sq = orig + pincr + delta;
            
                if (sq88_is_ok(sq))
                    PawnAttacks[side][i] |= bb::bit88(sq);
            }
        }
       
        // Knight attacks
        
        for (auto incr : IncrsN) {
            sq = orig + incr;

            if (sq88_is_ok(sq))
                PieceAttacks[Knight][i] |= bb::bit88(sq);
        }

        // Bishop attacks

        for (auto incr : IncrsB) {
            for (sq = orig + incr; sq88_is_ok(sq); sq += incr)
                PieceAttacks[Bishop][i] |= bb::bit88(sq);
        }
        
        // Rook attacks
        
        for (auto incr : IncrsR) {
            for (sq = orig + incr; sq88_is_ok(sq); sq += incr)
                PieceAttacks[Rook][i] |= bb::bit88(sq);
        }

        // Queen attacks
        
        for (auto incr : IncrsQ) {
            for (sq = orig + incr; sq88_is_ok(sq); sq += incr)
                PieceAttacks[Queen][i] |= bb::bit88(sq);
        }

        // King attacks

        for (auto incr : IncrsQ) {
            sq = orig + incr;

            if (sq88_is_ok(sq))
                PieceAttacks[King][i] |= bb::bit88(sq);
        }
    }
    
    for (int i = 0; i < 64; i++) {
        const int sq1 = to_sq88(i);

        for (int j = 0; j < 64; j++) {
            if (i == j) continue;

            int sq2 = to_sq88(j);

            if (!pseudo_attack(sq2, sq1, QueenFlags256)) continue;

            int incr = delta_incr(sq2, sq1);

            for (sq2 += incr; sq2 != sq1; sq2 += incr)
                InBetween[i][j] |= bb::bit88(sq2);
        }
    }
}
