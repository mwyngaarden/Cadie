#include <bit>
#include <cassert>
#include <cmath>
#include "eval.h"
#include "htable.h"
#include "pawn.h"

using namespace std;

HashTable<1024, PawnEntry> phtable;

enum { FileClosed, FileSemi, FileOpen };

template <int Side, int pincr> static int pawn_count(const Position& pos, int orig);

template <int Side> static int pawn_semiopen    (const Position& pos, int orig);
template <int Side> static int pawn_support     (const Position& pos, int orig);
template <int Side> static int pawn_phalanx     (const Position& pos, int orig);
template <int Side> static int pawn_opposed     (const Position& pos, int orig);
template <int Side> static int pawn_attacks     (const Position& pos, int orig);

template <int Side> static bool pawn_doubled    (const Position& pos, int orig);
template <int Side> static bool pawn_backward   (const Position& pos, int orig);
template <int Side> static bool pawn_isolated   (const Position& pos, int orig);
template <int Side> static bool pawn_passed     (const Position& pos, int orig);

template <int Side> static Value eval_passed    (const Position& pos, int orig);
template <int Side> static Value eval_pawn      (const Position& pos, int orig);

template <int Side, int pincr>
int pawn_count(const Position& pos, int orig)
{
    constexpr u8 pawn = make_pawn(Side);

    int count = 0;

    for (int sq = orig; sq >= A2 && sq <= H7; sq += pincr)
        count += pos[sq] == pawn;

    return count;
}

template <int Side>
int pawn_semiopen(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr u8 opawn  = flip_pawn(mpawn);
    constexpr int pincr = pawn_incr(Side);

    int mp = 0;
    int op = 0;

    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr) {
        const u8 piece = pos[sq];

        mp += piece == mpawn;
        op += piece == opawn;
    }

    if (mp == 0 && op == 0)
        return FileOpen;
    else if (op > 0 && mp == 0)
        return FileSemi;
    else
        return FileClosed;
}

template <int Side>
bool pawn_passed(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    //for (int sq = orig + pincr; sq88_rank(sq, Side) <= Rank7; sq += pincr)
    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr)
        if (pos[sq - 1] == opawn || pos[sq + 1] == opawn)
            return false;

    return true;
}

template <int Side>
int pawn_opposed(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    int opposed = 0;

    //for (int sq = orig + pincr; sq88_rank(sq, Side) <= Rank7; sq += pincr)
    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr)
        opposed += pos[sq] == opawn;

    return opposed;
}

template <int Side>
int pawn_attacks(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    return (pos[orig + pincr - 1] == opawn) + (pos[orig + pincr + 1] == opawn);
}

template <int Side>
int pawn_phalanx(const Position& pos, int orig)
{
    constexpr u8 mpawn = make_pawn(Side);

    return (pos[orig - 1] == mpawn) + (pos[orig + 1] == mpawn);
}

template <int Side>
int pawn_support(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr int pincr = pawn_incr(Side);

    return (pos[orig - pincr - 1] == mpawn) + (pos[orig - pincr + 1] == mpawn);
}

template <int Side>
bool pawn_doubled(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr int pincr = pawn_incr(Side);

    //for (int sq = orig - pincr; sq88_rank(sq, Side) != Rank1; sq -= pincr)
    for (int sq = orig - pincr; sq >= A2 && sq <= H7; sq -= pincr)
        if (pos[sq] == mpawn)
            return true;

    return false;
}

template <int Side>
bool pawn_backward(const Position& pos, int orig)
{
    constexpr int mside =           Side;
    constexpr int oside = flip_side(Side);
    constexpr u8 mpawn  = make_pawn(mside);
    constexpr u8 opawn  = make_pawn(oside);
    constexpr int pincr = pawn_incr(mside);
    const int rank      = sq88_rank(orig, Side);

    if (rank == Rank7) return false;

    for (int sq = orig; sq >= A2 && sq <= H7; sq -= pincr)
        if (pos[sq - 1] == mpawn || pos[sq + 1] == mpawn)
            return false;
   
    // Blocked by pawn
    
    if (is_pawn(pos[orig + pincr]))
        return true;

    const int mprank1 = (pos[orig+1*pincr-1] == mpawn) + (pos[orig+1*pincr+1] == mpawn);
    const int oprank1 = (pos[orig+1*pincr-1] == opawn) + (pos[orig+1*pincr+1] == opawn);
    const int mprank2 = (pos[orig+2*pincr-1] == mpawn) + (pos[orig+2*pincr+1] == mpawn);
    const int oprank2 = (pos[orig+2*pincr-1] == opawn) + (pos[orig+2*pincr+1] == opawn);

    if (oprank1 || oprank2)
        return true;
    
    if (mprank1) return false;

    if (rank != Rank2 || is_pawn(pos[orig + 2 * pincr]) || !mprank2)
        return true;
    
    const int oprank3 = (pos[orig+3*pincr-1] == opawn) + (pos[orig+3*pincr+1] == opawn);
    
    return oprank3;
}

template <int Side>
bool pawn_isolated(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    const int file      = sq88_file(orig);

    //for (int sq = to_sq88(file, Rank2); sq88_rank(sq) != Rank8; sq += 16)
    for (int sq = to_sq88(file, Rank2); sq >= A2 && sq <= H7; sq += 16)
        if (pos[sq - 1] == mpawn || pos[sq + 1] == mpawn)
            return false;

    return true;
}

template <int Side>
Value eval_passed(const Position& pos, int orig)
{
    constexpr int pincr = pawn_incr(Side);
    const int mksq      = pos.king_sq(Side);
    const int oksq      = pos.king_sq(flip_side(Side));
    const int dest      = orig + pincr;
    const int mkdist    = sq88_dist(dest, mksq);
    const int okdist    = sq88_dist(dest, oksq);
    const int rank      = sq88_rank(orig, Side);
    
    const int weight[8] = { 0, 0, 0, 10, 30, 60, 100, 0 };
    const int w = weight[rank];

    const int dbonus = PawnPassedFactor1.mg * okdist - 5 * mkdist;
    const int fbonus = PawnPassedFactor1.eg * (pos.empty(dest) && !pos.side_attacks(flip_side(Side), dest));

    const int mg = PawnPassedBase.mg +  PawnPassedFactor2.mg                    * w / 100;
    const int eg = PawnPassedBase.eg + (PawnPassedFactor2.eg + dbonus + fbonus) * w / 100;

    return Value { mg, eg };
}

template <int Side>
Value eval_pawn(const Position& pos, int orig, PawnEntry& pentry)
{
    Value val;
    
    val.mg += ValuePawn[PhaseMg];
    val.eg += ValuePawn[PhaseEg];
    val.mg += PSQT[PhaseMg][WP12 + Side][orig];
    val.eg += PSQT[PhaseEg][WP12 + Side][orig];

    constexpr int mside =           Side;
    constexpr int oside = flip_side(Side);
    constexpr int pincr = pawn_incr(Side);
    // constexpr u8 opawn  = make_pawn(oside);
    const int rank      = sq88_rank(orig, Side);
        
    bool backward   = false;
    bool isolated   = false;
    // bool blocked    = false;
    bool passed     = false;
    bool candidate  = false;

    int open        = 0;
    int opposed     = 0;

    if ((open = pawn_semiopen<Side>(pos, orig)) == FileOpen) {
        passed = pawn_passed<Side>(pos, orig);

        if (passed)
            pentry.passed ^= 1ull << to_sq64(orig);
    }
    else {
        opposed = pawn_opposed<Side>(pos, orig);

        //if (opposed)
            //blocked = pos[orig + pincr] == opawn;
    }

    const int phalanx   = pawn_phalanx<Side>(pos, orig);
    const int support   = pawn_support<Side>(pos, orig);
    const int attacks   = pawn_attacks<Side>(pos, orig);
    const bool doubled  = pawn_doubled<Side>(pos, orig);

    if (support == 0 && phalanx == 0) {
        isolated = pawn_isolated<Side>(pos, orig);

        if (!isolated)
            backward = pawn_backward<Side>(pos, orig);
    }
    
    assert(!(isolated && backward));

    // Candidate

    if (open == FileOpen && !passed) {
        if (support >= attacks) {
            int mp = support + phalanx;
            int op = attacks;

            mp += pawn_count<mside, -pincr>(pos, orig-2*pincr-1);
            mp += pawn_count<mside, -pincr>(pos, orig-2*pincr+1);
            op += pawn_count<oside,  pincr>(pos, orig+2*pincr-1);
            op += pawn_count<oside,  pincr>(pos, orig+2*pincr+1);

            candidate = mp >= op;

            if (candidate) {
                const int weight[8] = { 0, 0, 0, 10, 30, 60, 100, 0 };
                const int w = weight[rank];

                const int bmg = PawnCandidateBase.mg;
                const int beg = PawnCandidateBase.eg;
                const int fmg = PawnCandidateFactor.mg;
                const int feg = PawnCandidateFactor.eg;

                val += Value { bmg + fmg * w / 100, beg + feg * w / 100 };
            } 
        }
    }

    // bonuses

    if (phalanx || support) {
        constexpr int cbonus[8] = { 0, 0, 5, 10, 15, 35, 75, 0 };

        const int v = cbonus[rank] * (2 + !!phalanx - !!opposed) + 10 * support;

        val += Value { v, v * (rank - 2) / 16 };
    }

    // penalties

    val -= doubled * PawnDoubledPenalty;

    if (isolated) val -= open ? PawnIsoOpenPenalty : PawnIsolatedPenalty;
    if (backward) val -= open ? PawnBackOpenPenalty : PawnBackwardPenalty;

    return val;
}

Value eval_passers(const Position& pos, PawnEntry& pentry)
{
    Value white;
    Value black;

    u64 passed = pentry.passed;

    while (passed) {
        const int orig = to_sq88(countr_zero(passed));
        
        passed &= passed - 1;

        const u8 pawn = pos[orig];

        assert(is_pawn(pawn));

        if (is_white(pawn)) 
            white += eval_passed<White>(pos, orig);
        else 
            black += eval_passed<Black>(pos, orig);
    }

    return white - black;
}

Value eval_pawns(const Position& pos, PawnEntry& pentry)
{
    if (phtable.get(pos.pawn_key(), pentry))
        return Value { pentry.mg, pentry.eg };
    
    Value val;

    for (int orig : pos.plist(WP12)) val += eval_pawn<White>(pos, orig, pentry);
    for (int orig : pos.plist(BP12)) val -= eval_pawn<Black>(pos, orig, pentry);

    pentry.mg = val.mg;
    pentry.eg = val.eg;

    phtable.set(pos.pawn_key(), pentry);

    return val;
}
