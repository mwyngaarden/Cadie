#include <bit>
#include <cassert>
#include <cmath>
#include "attack.h"
#include "bb.h"
#include "eval.h"
#include "ht.h"
#include "pawn.h"
#include "see.h"

using namespace std;

template <side_t Side> static bool pawn_backward   (const Position& pos, int orig);
template <side_t Side> static Value eval_passed    (const Position& pos, int orig);
template <side_t Side> static Value eval_pawn      (const Position& pos, int orig);

template <side_t Side>
bool unstoppable_passer(const Position& pos, int orig)
{
    constexpr int incr = square::incr(Side);

    int rank_rel = square::rank(orig, Side);
    int rank     = square::rank(orig);

    int king = pos.king(!Side);
    
    if (pos.bb(Side) & bb::PawnSpan[Side][rank])
        return false;

    if (rank_rel == Rank2) {
        orig += incr;
        ++rank_rel;
        
        assert(rank_rel == square::rank(orig, Side));
    }
        
    assert(rank_rel >= Rank3 && rank_rel <= Rank7);

    int prom = square::pawn_promo(Side, orig);

    int dist = square::dist(orig, prom);

    assert(dist == Rank8 - rank_rel);

    if (pos.side() == side_t(!Side)) dist++;

    return square::dist(king, prom) > dist;
}

template <side_t Side>
bool king_passer(const Position& pos, int orig)
{
    int king = pos.king(Side);
    int file = square::file(orig);

    int prom = square::pawn_promo(Side, orig);

    if (   square::dist(king, prom) <= 1
        && square::dist(king, orig) <= 1
        && (square::file(king) != file || (file != FileA && file != FileH)))
        return true;

    return false;
}

template <side_t Side>
bool free_passer(const Position& pos, int orig)
{
    constexpr int incr = square::incr(Side);

    int sq = orig + incr;

    if (!pos.empty(sq)) return false;

    Move mv(orig, sq);
   
    return see::calc(pos, mv) > 0;
}

template <side_t Side>
bool pawn_backward(const Position& pos, int orig)
{
    constexpr int incr = square::incr(Side);

    int rank     = square::rank(orig);
    int rank_rel = square::rank(orig, Side);
    int file     = square::file(orig);

    if (rank_rel == Rank7) return false;

    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);

    u64 pbb = mpbb | opbb;
    
    mpbb &= bb::FilesAdj[file];
    opbb &= bb::FilesAdj[file];

    if (mpbb & ~(Side == White ? bb::RanksGT[rank] : bb::RanksLT[rank]))
        return false;

    if (bb::test(pbb, orig + incr))
        return true;

    u64 p1rank = bb::Ranks[rank+(Side==White?1:-1)];
    u64 p2rank = bb::Ranks[rank+(Side==White?2:-2)];

    if (opbb & (p1rank | p2rank)) return true;

    if (mpbb & p1rank) return false;

    if (rank_rel != Rank2) return true;

    if (sq64_is_ok(orig + 2 * incr) && bb::test(pbb, orig + 2 * incr)) return true;
    
    if ((mpbb & p2rank) == 0)
        return true;

    return rank_rel != Rank6 && (opbb & bb::Ranks[rank+(Side==White?3:-3)]);
}

template <side_t Side>
Value eval_passed(const Position& pos, int orig)
{
    constexpr int incr = square::incr(Side);

    int mksq    = pos.king(Side);
    int oksq    = pos.king(!Side);
    int dest    = orig + incr;
    int mkdist  = square::dist(dest, mksq);
    int okdist  = square::dist(dest, oksq);
    int rank    = square::rank(orig, Side);

    Value val;

    int idx = 0;

    if (!pos.pieces(!Side) && (unstoppable_passer<Side>(pos, orig) || king_passer<Side>(pos, orig)))
        idx = 1;
    else if (free_passer<Side>(pos, orig))
        idx = 2;
    
    if (idx != 1) {
        u64 occ    = pos.occ();
        u64 king   = pos.bb(Side, King);
        u64 pawns  = pos.bb(Side, Pawn);
        u64 minors = pos.minors(Side);
        u64 majors = pos.majors(Side);

        u64 rspan = bb::PawnSpan[!Side][orig];

        for (u64 bb = rspan & majors; bb; ) {
            int sq = bb::pop(bb);

            u64 between = bb::InBetween[sq][orig] & occ;

            if (between & pawns) continue;

            u64 blockers = between & (minors | king);

            if (!blockers) {
                val.mg += PawnPassedGaurdedBm;      
                val.eg += PawnPassedGaurdedBe;      
            }
        }
    }

    int mkdist_mg = PawnPassedB[PhaseMg][idx][0];
    int okdist_mg = PawnPassedB[PhaseMg][idx][7];
    int mkdist_eg = PawnPassedB[PhaseEg][idx][0];
    int okdist_eg = PawnPassedB[PhaseEg][idx][7];

    int kdist_bonus_mg = okdist_mg * okdist - mkdist_mg * mkdist;
    int kdist_bonus_eg = okdist_eg * okdist - mkdist_eg * mkdist;

    val.mg += PawnPassedB[PhaseMg][idx][rank] + kdist_bonus_mg;
    val.eg += PawnPassedB[PhaseEg][idx][rank] + kdist_bonus_eg;

    return val;
}

template <side_t Side>
Value eval_pawn(const Position& pos, int orig)
{
    Value val;

    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);
    u64 adjbb = bb::FilesAdj[square::file(orig)];
    u64 span = bb::PawnSpan[Side][orig];
    u64 span_adj = bb::PawnSpanAdj[Side][orig];
    u64 rspan_adj = bb::PawnSpanAdj[!Side][orig];
    
    int rank     = square::rank(orig);
    int rank_rel = square::rank(orig, Side);
        
    bool backward   = false;
    bool isolated   = false;
    bool passed     = false;
    bool candidate  = false;
    bool opposed    = false;
   
    bool open = (span & (mpbb | opbb)) == 0;

    if (open) {
        passed = !(opbb & span_adj);

        if (passed)
            val += eval_passed<Side>(pos, orig);
    }
    else
        opposed = opbb & span;

    int phalanx = bb::pawns2(mpbb & adjbb & bb::Ranks[rank]);
    int support = bb::pawns2(mpbb & PawnAttacks[!Side][orig]);
    int attacks = bb::pawns2(opbb & PawnAttacks[ Side][orig]);

    bool doubled = mpbb & bb::PawnSpan[!Side][orig];

    if (support == 0 && phalanx == 0) {
        isolated = !(mpbb & adjbb);

        if (!isolated)
            backward = pawn_backward<Side>(pos, orig);
    }
    
    assert(!(isolated && backward));

    // Candidate

    if (open && !passed) {
        assert(rank_rel < Rank7);

        if (support >= attacks) {
            int mp = popcount(mpbb & rspan_adj) + phalanx;
            int op = popcount(opbb & span_adj);

            candidate = mp >= op;

            if (candidate)
                val.eg += PawnCandidateB[rank_rel];
        }
    }

    // Bonuses

    if (phalanx || support) {
        int v = PawnConnectedB[rank_rel] * (2 + !!phalanx - opposed) + PawnConnectedPB * support;

        val.mg += v;
        val.eg += v * (rank_rel - 2) / 4;
    }

    for (u64 bb = PawnAttacks[Side][orig] & pos.pieces(!Side); bb; ) {
        int sq = bb::pop(bb);

        int type = P256ToP6[pos.board(sq)];

        val.mg += PawnAttackB[PhaseMg][type];
        val.eg += PawnAttackB[PhaseEg][type];
    }

    // Penalties

    val.mg += doubled * PawnDoubledPm;
    val.eg += doubled * PawnDoubledPe;

    if (isolated) {
        val.mg += open ? PawnIsoOpenPm : PawnIsolatedPm;
        val.eg += open ? PawnIsoOpenPe : PawnIsolatedPe;
    }
    else if (backward) {
        val.mg += open ? PawnBackOpenPm : PawnBackwardPm;
        val.eg += open ? PawnBackOpenPe : PawnBackwardPe;
    }

    return val;
}

Value eval_pawns(const Position& pos)
{
    Value val;

    for (u64 bb = pos.bb(White, Pawn); bb; ) {
        int sq = bb::pop(bb);
        val += eval_pawn<White>(pos, sq);
    }
    
    for (u64 bb = pos.bb(Black, Pawn); bb; ) {
        int sq = bb::pop(bb);
        val -= eval_pawn<Black>(pos, sq);
    }

    return val;
}
