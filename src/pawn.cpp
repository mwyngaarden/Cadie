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

enum { FileClosed, FileSemi, FileOpen };

template <side_t Side> static int pawn_semiopen    (const Position& pos, int orig);
template <side_t Side> static bool pawn_backward   (const Position& pos, int orig);
template <side_t Side> static Value eval_passed    (const Position& pos, int orig);
template <side_t Side> static Value eval_pawn      (const Position& pos, int orig);

template <side_t Side>
int pawn_semiopen(const Position& pos, int orig)
{
    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);

    u64 bb = bb::PawnSpan[Side][orig];

    int mp = popcount(mpbb & bb);
    int op = popcount(opbb & bb);

    if (mp == 0 && op == 0)
        return FileOpen;
    else if (mp == 0 && op > 0)
        return FileSemi;
    else
        return FileClosed;
}

template <side_t Side>
bool pawn_backward(const Position& pos, int orig)
{
    constexpr int pincr = Side == White ? 8 : -8;

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

    if (bb::test(pbb, orig + pincr))
        return true;

    u64 p1rank = bb::Ranks[rank+(Side==White?1:-1)];
    u64 p2rank = bb::Ranks[rank+(Side==White?2:-2)];

    if (opbb & (p1rank | p2rank)) return true;

    if (mpbb & p1rank) return false;

    if (rank_rel != Rank2) return true;

    if (sq64_is_ok(orig + 2 * pincr) && bb::test(pbb, orig + 2 * pincr)) return true;
    
    if ((mpbb & p2rank) == 0)
        return true;

    return rank_rel != Rank6 && (opbb & bb::Ranks[rank+(Side==White?3:-3)]);
}

template <side_t Side>
Value eval_passed(const Position& pos, int orig)
{
    constexpr int pincr = Side == White ? 8 : -8;

    int mksq    = pos.king64(Side);
    int oksq    = pos.king64(!Side);
    int dest    = orig + pincr;
    int mkdist  = square::dist(dest, mksq);
    int okdist  = square::dist(dest, oksq);
    int rank    = square::rank(orig, Side);

    double weight[8] = { 0, 0, 0, 0.1, 0.3, 0.6, 1, 0 };

    double w = weight[rank];

    int dbonus = PawnPassedFactorA * okdist - PawnPassedFactorB * mkdist;
    int fbonus = PawnPassedFactorC * (pos.board(dest) == PieceNone256 && see::calc(pos, Move(to_sq88(orig), to_sq88(dest))));

    int bmg = PawnPassedBaseMg;
    int fmg = PawnPassedFactorMg;
    int beg = PawnPassedBaseEg;
    int feg = PawnPassedFactorEg;

    int mg = bmg + w * fmg;
    int eg = beg + w * (feg + dbonus + fbonus);

    return Value { mg, eg };
}

template <side_t Side>
Value eval_pawn(const Position& pos, int orig, PawnEntry& pentry)
{
    Value val;

    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);
    u64 adjbb = bb::FilesAdj[square::file(orig)];
    
    int rank     = square::rank(orig);
    int rank_rel = square::rank(orig, Side);
        
    bool backward   = false;
    bool isolated   = false;
    bool passed     = false;
    bool candidate  = false;
    bool opposed    = false;

    int open        = 0;

    if ((open = pawn_semiopen<Side>(pos, orig)) == FileOpen) {
        passed = !(opbb & bb::PawnSpanAdj[Side][orig]);

        if (passed)
            pentry.passed |= bb::bit(orig);
    }
    else {
        opposed = opbb & bb::PawnSpan[Side][orig];
    }

    int phalanx = popcount(mpbb & adjbb & bb::Ranks[rank]);
    int support = popcount(mpbb & PawnAttacks[!Side][orig]);
    int attacks = popcount(opbb & PawnAttacks[ Side][orig]);

    bool doubled  = mpbb & bb::PawnSpan[!Side][orig];

    if (support == 0 && phalanx == 0) {
        isolated = !(mpbb & adjbb);

        if (!isolated)
            backward = pawn_backward<Side>(pos, orig);
    }
    
    assert(!(isolated && backward));

    // Candidate

    if (open == FileOpen && !passed) {
        if (support >= attacks) {
            int mp = popcount(mpbb & bb::PawnSpanAdj[!Side][orig]) + phalanx;
            int op = popcount(opbb & bb::PawnSpanAdj[ Side][orig]);

            candidate = mp >= op;

            if (candidate) {
                double weight[8] = { 0, 0, 0, 0.03, 0.21, 0.75, 1, 0 };

                double w = weight[rank_rel];

                int bmg = PawnCandidateBaseMg;
                int fmg = PawnCandidateFactorMg;

                int beg = PawnCandidateBaseEg;
                int feg = PawnCandidateFactorEg;

                val.mg += bmg + w * fmg;
                val.eg += beg + w * feg;
            } 
        }
    }

    // bonuses

    if (phalanx || support) {
        constexpr int cbonus[8] = { 0, 0, 2, 5, 15, 31, 40, 0 };

        int v = cbonus[rank_rel] * (2 + !!phalanx - opposed) + 6 * support;

        val.mg += v;
        val.eg += v * (rank_rel - 2) / 4;
    }

    const int weight[6] = { 0, 1, 1, 2, 4, 0 };

    for (u64 bb = PawnAttacks[Side][orig] & pos.occ(!Side); bb; ) {
        int sq = bb::pop(bb);

        u8 piece = pos.board(sq);

        int type = P256ToP6[piece];

        val.mg += PawnAttackMg * weight[type];
        val.eg += PawnAttackEg * weight[type];
    }

    // penalties

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

Value eval_passers(const Position& pos, PawnEntry& pentry)
{
    Value val;

    u64 wbb = pos.occ(White);

    for (u64 bb = pentry.passed; bb; ) {
        int sq = bb::pop(bb);

        if (bb::test(wbb, sq))
            val += eval_passed<White>(pos, sq);
        else 
            val -= eval_passed<Black>(pos, sq);
    }

    return val;
}

Value eval_pawns(const Position& pos, PawnEntry& pentry)
{
    if (ptable.get(pos.pawn_key(), pentry))
        return Value { pentry.mg, pentry.eg };
    
    Value val;

    int sq;
    u64 bb;

    for (bb = pos.bb(White, Pawn); bb; ) {
        sq = bb::pop(bb);
        val += eval_pawn<White>(pos, sq, pentry);
    }
    
    for (bb = pos.bb(Black, Pawn); bb; ) {
        sq = bb::pop(bb);
        val -= eval_pawn<Black>(pos, sq, pentry);
    }

    pentry.mg = val.mg;
    pentry.eg = val.eg;

    ptable.set(pos.pawn_key(), pentry);

    return val;
}
