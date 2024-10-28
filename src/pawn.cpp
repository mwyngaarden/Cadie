#include <bit>
#include <cassert>
#include <cmath>
#include "attacks.h"
#include "bb.h"
#include "eval.h"
#include "ht.h"
#include "pawn.h"

using namespace std;

template <side_t Side> static Value eval_passed    (const Position& pos, int orig);
template <side_t Side> static Value eval_pawn      (const Position& pos, int orig);

template <side_t Side>
bool unstoppable_passer(const Position& pos, int orig)
{
    constexpr int incr = square::incr(Side);

    int king = pos.king(!Side);
    int rank = square::rank(orig, Side);
    
    if (pos.bb(Side) & bb::PawnSpan[Side][orig])
        return false;

    if (rank == Rank2) {
        orig += incr;
        rank++;
        
        assert(rank == square::rank(orig, Side));
    }
        
    assert(rank >= Rank3 && rank <= Rank7);

    int prom = square::pawn_promo(Side, orig);

    int dist = square::dist(orig, prom);

    assert(dist == Rank8 - rank);

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

// TODO refactor
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
    else if (pos.empty(dest) && !pos.side_attacks(!Side, dest))
        idx = 2;
    
    if (idx != 1) {
        u64 occ    = pos.occ();
        u64 majors = pos.majors(Side);
        u64 rspan  = bb::PawnSpan[!Side][orig];

        for (u64 bb = rspan & pos.bb(Side, Rook); bb; ) {
            int sq = bb::pop(bb);

            u64 between = bb::Between[sq][orig] & occ;

            if ((between & ~majors) == 0)
                val += Value(PawnPassedGaurdedBm, PawnPassedGaurdedBe);
        }

        int psq   = square::pawn_promo(Side, orig);
        bool mdef = pos.side_attacks( Side, psq);
        bool oatt = pos.side_attacks(!Side, psq);

        if (mdef && !oatt)
            val.eg += PawnPassedStopOffset;
        else if (!mdef && oatt)
            val.eg -= PawnPassedStopOffset;
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
    constexpr int incr = square::incr(Side);

    Value val;

    u64 mpbb        = pos.bb(Side, Pawn);
    u64 opbb        = pos.bb(!Side, Pawn);
    u64 adjbb       = bb::FilesAdj[square::file(orig)];
    u64 span        = bb::PawnSpan[Side][orig];
    u64 span_adj    = bb::PawnSpanAdj[Side][orig];
    u64 rspan_adj   = bb::PawnSpanAdj[!Side][orig];

    int rank        = square::rank(orig);
    int rank_rel    = square::rank(orig, Side);

    bool backward   = false;
    bool isolated   = false;
    bool passed     = false;
    bool opposed    = false;

    bool open = !(span & (mpbb | opbb));

    if (open) {
        passed = !(opbb & span_adj);

        if (passed)
            val += eval_passed<Side>(pos, orig);
    }
    else
        opposed = opbb & span;

    int phalanx = popcount(mpbb & adjbb & bb::Ranks[rank]);
    int support = popcount(mpbb & PawnAttacks[!Side][orig]);
    int levers  = popcount(opbb & PawnAttacks[ Side][orig]);

    bool doubled = mpbb & bb::PawnSpan[!Side][orig];
    bool blocked = !pos.empty(orig + incr);

    if (support == 0 && phalanx == 0) {
        isolated = !(mpbb & adjbb);

        if (!isolated) {
            u64 leversp = opbb & PawnAttacks[Side][orig + incr];

            backward = !(mpbb & rspan_adj) && (leversp || bb::test(opbb, orig + incr));
        }
    }

    assert(!(isolated && backward));

    // Candidate

    if (open && !passed) {
        assert(rank_rel < Rank7);

        if (support >= levers) {
            int mp = popcount(mpbb & rspan_adj) + phalanx;
            int op = popcount(opbb & span_adj);

            if (mp >= op)
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

        int type = pos.board<6>(sq);

        val.mg += PawnAttackB[PhaseMg][!!support][type];
        val.eg += PawnAttackB[PhaseEg][!!support][type];
    }

    if (!blocked) {
        for (u64 bb = PawnAttacks[Side][orig + incr] & pos.pieces(!Side); bb; ) {
            int sq = bb::pop(bb);

            int type = pos.board<6>(sq);

            val.mg += PawnAttackPushB[PhaseMg][!!phalanx][type];
            val.eg += PawnAttackPushB[PhaseEg][!!phalanx][type];
        }
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

    u64 wpawns = pos.bb(White, Pawn);
    u64 bpawns = pos.bb(Black, Pawn);

    while (wpawns) {
        int sq = bb::pop(wpawns);
        val += eval_pawn<White>(pos, sq);
    }

    while (bpawns) {
        int sq = bb::pop(bpawns);
        val -= eval_pawn<Black>(pos, sq);
    }

    return val;
}
