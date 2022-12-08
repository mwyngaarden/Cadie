#include <bit>
#include <cassert>
#include <cmath>
#include "eval.h"
#include "htable.h"
#include "pawn.h"

using namespace std;

HashTable<1024, PawnEntry> phtable;

enum { FileClosed, FileSemi, FileOpen };

template <side_t Side, int pincr> static int pawn_count(const Position& pos, int orig);

template <side_t Side> static int pawn_semiopen    (const Position& pos, int orig);
template <side_t Side> static int pawn_support     (const Position& pos, int orig);
template <side_t Side> static int pawn_phalanx     (const Position& pos, int orig);
template <side_t Side> static int pawn_opposed     (const Position& pos, int orig);
template <side_t Side> static int pawn_attacks     (const Position& pos, int orig);

template <side_t Side> static bool pawn_doubled    (const Position& pos, int orig);
template <side_t Side> static bool pawn_backward   (const Position& pos, int orig);
template <side_t Side> static bool pawn_isolated   (const Position& pos, int orig);
template <side_t Side> static bool pawn_passed     (const Position& pos, int orig);

template <side_t Side> static Value eval_passed    (const Position& pos, int orig);
template <side_t Side> static Value eval_pawn      (const Position& pos, int orig);

template <side_t Side, int pincr>
int pawn_count(const Position& pos, int orig)
{
    constexpr u8 pawn = make_pawn(Side);

    int count = 0;

    for (int sq = orig; sq >= A2 && sq <= H7; sq += pincr)
        count += pos[sq] == pawn;

    return count;
}

template <side_t Side>
int pawn_semiopen(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr u8 opawn  = flip_pawn(mpawn);
    constexpr int pincr = pawn_incr(Side);

    int mp = 0;
    int op = 0;

    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr) {
        u8 piece = pos[sq];

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

template <side_t Side>
bool pawn_passed(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr)
        if (pos[sq - 1] == opawn || pos[sq + 1] == opawn)
            return false;

    return true;
}

template <side_t Side>
int pawn_opposed(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    int opposed = 0;

    for (int sq = orig + pincr; sq >= A2 && sq <= H7; sq += pincr)
        opposed += pos[sq] == opawn;

    return opposed;
}

template <side_t Side>
int pawn_attacks(const Position& pos, int orig)
{
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    constexpr int pincr = pawn_incr(Side);

    return (pos[orig + pincr - 1] == opawn) + (pos[orig + pincr + 1] == opawn);
}

template <side_t Side>
int pawn_phalanx(const Position& pos, int orig)
{
    constexpr u8 mpawn = make_pawn(Side);

    return (pos[orig - 1] == mpawn) + (pos[orig + 1] == mpawn);
}

template <side_t Side>
int pawn_support(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr int pincr = pawn_incr(Side);

    return (pos[orig - pincr - 1] == mpawn) + (pos[orig - pincr + 1] == mpawn);
}

template <side_t Side>
bool pawn_doubled(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr int pincr = pawn_incr(Side);

    for (int sq = orig - pincr; sq >= A2 && sq <= H7; sq -= pincr)
        if (pos[sq] == mpawn)
            return true;

    return false;
}

template <side_t Side>
bool pawn_backward(const Position& pos, int orig)
{
    constexpr side_t mside =            Side;
    constexpr side_t oside =  flip_side(Side);
    constexpr u8 mpawn  = make_pawn(mside);
    constexpr u8 opawn  = make_pawn(oside);
    constexpr int pincr = pawn_incr(mside);
    int rank      = sq88_rank(orig, Side);

    if (rank == Rank7) return false;

    for (int sq = orig; sq >= A2 && sq <= H7; sq -= pincr)
        if (pos[sq - 1] == mpawn || pos[sq + 1] == mpawn)
            return false;
   
    // Blocked by pawn
    
    if (is_pawn(pos[orig + pincr]))
        return true;

    int mprank1 = (pos[orig+1*pincr-1] == mpawn) + (pos[orig+1*pincr+1] == mpawn);
    int oprank1 = (pos[orig+1*pincr-1] == opawn) + (pos[orig+1*pincr+1] == opawn);
    int mprank2 = (pos[orig+2*pincr-1] == mpawn) + (pos[orig+2*pincr+1] == mpawn);
    int oprank2 = (pos[orig+2*pincr-1] == opawn) + (pos[orig+2*pincr+1] == opawn);

    if (oprank1 || oprank2)
        return true;
    
    if (mprank1) return false;

    if (rank != Rank2 || is_pawn(pos[orig + 2 * pincr]) || !mprank2)
        return true;
    
    int oprank3 = (pos[orig+3*pincr-1] == opawn) + (pos[orig+3*pincr+1] == opawn);
    
    return oprank3;
}

template <side_t Side>
bool pawn_isolated(const Position& pos, int orig)
{
    constexpr u8 mpawn  = make_pawn(Side);
    int file      = sq88_file(orig);

    for (int sq = to_sq88(file, Rank2); sq >= A2 && sq <= H7; sq += 16)
        if (pos[sq - 1] == mpawn || pos[sq + 1] == mpawn)
            return false;

    return true;
}

template <side_t Side>
Value eval_passed(const Position& pos, int orig)
{
    constexpr int pincr = pawn_incr(Side);
    int mksq      = pos.king_sq(Side);
    int oksq      = pos.king_sq(flip_side(Side));
    int dest      = orig + pincr;
    int mkdist    = sq88_dist(dest, mksq);
    int okdist    = sq88_dist(dest, oksq);
    int rank      = sq88_rank(orig, Side);
    
    int weight[8] = { 0, 0, 0, 10, 30, 60, 100, 0 };
    int w = weight[rank];

    int dbonus = PawnPassedFactor1.mg * okdist - 5 * mkdist;
    int fbonus = PawnPassedFactor1.eg * (pos.empty(dest) && !pos.side_attacks(flip_side(Side), dest));

    int mg = PawnPassedBase.mg +  PawnPassedFactor2.mg                    * w / 100;
    int eg = PawnPassedBase.eg + (PawnPassedFactor2.eg + dbonus + fbonus) * w / 100;

    return Value { mg, eg };
}

template <side_t Side>
Value eval_pawn(const Position& pos, int orig, PawnEntry& pentry)
{
    Value val;
    
    val.mg += ValuePawn[PhaseMg];
    val.eg += ValuePawn[PhaseEg];
    val.mg += PSQT[PhaseMg][WP12 + Side][orig];
    val.eg += PSQT[PhaseEg][WP12 + Side][orig];

    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);
    constexpr int pincr = pawn_incr(Side);
    // constexpr u8 opawn  = make_pawn(oside);
    int rank      = sq88_rank(orig, Side);
        
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

    int phalanx   = pawn_phalanx<Side>(pos, orig);
    int support   = pawn_support<Side>(pos, orig);
    int attacks   = pawn_attacks<Side>(pos, orig);
    bool doubled  = pawn_doubled<Side>(pos, orig);

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
                int weight[8] = { 0, 0, 0, 10, 30, 60, 100, 0 };
                int w = weight[rank];

                int bmg = PawnCandidateBase.mg;
                int beg = PawnCandidateBase.eg;
                int fmg = PawnCandidateFactor.mg;
                int feg = PawnCandidateFactor.eg;

                val += Value { bmg + fmg * w / 100, beg + feg * w / 100 };
            } 
        }
    }

    // bonuses

    if (phalanx || support) {
        constexpr int cbonus[8] = { 0, 0, 5, 10, 15, 35, 75, 0 };

        int v = cbonus[rank] * (2 + !!phalanx - !!opposed) + 10 * support;

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
        int orig = to_sq88(countr_zero(passed));
        
        passed &= passed - 1;

        u8 pawn = pos[orig];

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
