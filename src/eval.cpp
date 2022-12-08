#include <algorithm>
#include <bit>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <vector>
#include <cassert>
#include <cmath>
#include <omp.h>
#include "eval.h"
#include "gen.h"
#include "htable.h"
#include "pawn.h"
#include "psqt.h"
#include "search.h"
using namespace std;

random_device rd;
mt19937 mt(rd());
//mt19937 mt(123456);

i16 PSQT[PhaseCount][12][128];

int MobBonus[12][256];

enum { FileClosed, FileSemi, FileOpen };

HashTable<1024 * 16, EvalEntry> ehtable;

constexpr int minor_outpost_bonus[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 4, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 4, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#if 0

#else

Value KnightMobFactor { 2, 7 };
Value KnightPawnBonus { 2, 6 };
Value BishopMobFactor { 3, 6 };
Value RookMobFactor { 1, 5 };
Value RookPawnBonus { 5, 1 };
Value QueenMobFactor { 1, 6 };
Value ValuePawn { 60, 118 };
Value ValueKnight { 328, 369 };
Value ValueBishop { 366, 403 };
Value ValueRook { 497, 597 };
Value ValueQueen { 1030, 1070 };
Value ValueKing { 0, 0 };
Value BishopBlockedPenalty { 45, 54 };
Value BishopTrappedPenalty { 82, 71 };
Value RookBlockedPenalty { 54, 40 };
Value PsqtPFactor { 42, 100 };
Value PsqtNFactor { 95, 133 };
Value PsqtBFactor { 87, 95 };
Value PsqtRFactor { 104, 108 };
Value PsqtQFactor { 100, 125 };
Value PsqtKFactor { 54, 93 };
Value PawnBackOpenPenalty { 10, 15 };
Value PawnBackwardPenalty { 10, -1 };
Value PawnCandidateBase { -4, 3 };
Value PawnCandidateFactor { 28, 68 };
Value PawnDoubledPenalty { -8, 12 };
Value PawnIsoOpenPenalty { 2, 17 };
Value PawnIsolatedPenalty { 15, 20 };
Value PawnPassedBase { -15, 25 };
Value PawnPassedFactor1 { 39, 125 };
Value PawnPassedFactor2 { 147, 46 };
Value KnightBehindPawnBonus { 4, 10 };
Value KnightOutpostBonus { 5, 8 };
Value BishopBehindPawnBonus { 0, 12 };
Value BishopPairBonus { 9, 77 };
Value BishopOutpostBonus { 11, 2 };
Value RookOpenBonus { -7, 43 };
Value RookSemiBonus { -26, 42 };
Value RookClosedPenalty { 30, -32 };
Value RookSemiAdjKingBonus { -8, 24 };
Value RookSemiSameKingBonus { 22, 5 };
Value RookOpenAdjKingBonus { 49, 35 };
Value RookOpenSameKingBonus { 39, 43 };
Value RookSeventhRankBonus { 13, 20 };
Value QueenSeventhRankBonus { -39, 39 };
Value KingOpenPenalty { 22, 41 };
Value KingSemiPenalty { -18, 46 };
Value KingSafetyFactor { 84, 100 };
Value ComplexityPawnsTotal { 0, 4 };
Value ComplexityPawnsFlanked { 0, 9 };
Value ComplexityPawnsEndgame { 0, 70 };
Value ComplexityAdjustment { 0, -23 };
Value ScaleDraw { 0, 0 };
Value ScaleNormal { 0, 128 };
Value ScaleOcbBishopsOnly { 0, 61 };
Value ScaleOcbOneKnight { 0, 117 };
Value ScaleOcbOneRook { 0, 118 };
Value ScaleLoneQueen { 0, 112 };
Value ScaleLargePawnAdv { 0, 157 };
Value ScaleB { 0, 92 };
Value ScaleM { 0, 18 };

#endif

// TODO tune

constexpr int KnightMobInit     =  -2; // T2 -4
constexpr int BishopMobInit     = -10; // T2 -6
constexpr int RookMobInit       =  -2; // T2 -7
constexpr int QueenMobInit      =  -2;

template <side_t Side> static int piece_on_semiopen (const Position& pos, int orig);
template <side_t Side> static int minor_on_outpost  (const Position& pos, int orig);

template <side_t Side> static Value eval_pieces     (const Position& pos);
template <side_t Side> static Value eval_knight     (const Position& pos, int orig);
template <side_t Side> static Value eval_bishop     (const Position& pos, int orig);
template <side_t Side> static Value eval_rook       (const Position& pos, int orig);
template <side_t Side> static Value eval_queen      (const Position& pos, int orig);
template <side_t Side> static Value eval_king       (const Position& pos);
template <side_t Side> static Value eval_shelter    (const Position& pos);
template <side_t Side> static Value eval_storm      (const Position& pos);
template <side_t Side> static Value eval_mate       (const Position& pos);

static Value eval_complexity    (const Position& pos, const Value& val);
static int eval_scale_factor    (const Position& pos, const Value& val);

static int pawns_on_file_a(const Position& pos);
static int pawns_on_file_h(const Position& pos);

void eval_init()
{
    const i16* psqt_values[2][6] = {
        { psqt_pawn_mg, psqt_knight_mg, psqt_bishop_mg, psqt_rook_mg, psqt_queen_mg, psqt_king_mg },
        { psqt_pawn_eg, psqt_knight_eg, psqt_bishop_eg, psqt_rook_eg, psqt_queen_eg, psqt_king_eg }
    };

    const Value *PsqtFactors[6] = {
        &PsqtPFactor,
        &PsqtNFactor,
        &PsqtBFactor,
        &PsqtRFactor,
        &PsqtQFactor,
        &PsqtKFactor
    };

    for (int phase = PhaseMg; phase < PhaseCount; phase++) {
        for (int p12 = WP12; p12 <= BK12; p12++) {
            for (int sq = A1; sq <= H8; sq++) {
                if (!sq88_is_ok(sq)) continue;

                side_t side = p12 & 1;
                int ptype   = p12 / 2;
                int sqref   = side ? sq88_reflect(sq) : sq;
                i16 svalue  = psqt_values[phase][ptype][sqref];
                int factor  = (*PsqtFactors[ptype])[phase];
            
                PSQT[phase][p12][sq] = svalue * factor / 100;
            }
        }
    }

    // Special case for knights
  
#if 1

    MobBonus[WN12][PieceNone256] = 1; //
    MobBonus[BN12][PieceNone256] = 1; //

    MobBonus[WP12][BP256] = 1;
    MobBonus[WP12][BN256] = 2;
    MobBonus[WP12][BB256] = 2;
    MobBonus[WP12][BR256] = 4;
    MobBonus[WP12][BQ256] = 8;
    
    MobBonus[WN12][BP256] = 1;
    MobBonus[WN12][BN256] = 1;
    MobBonus[WN12][BB256] = 1;
    MobBonus[WN12][BR256] = 2;
    MobBonus[WN12][BQ256] = 6;
    
    MobBonus[WB12][BP256] = 1;
    MobBonus[WB12][BN256] = 1;
    MobBonus[WB12][BB256] = 1;
    MobBonus[WB12][BR256] = 2;
    MobBonus[WB12][BQ256] = 6;
    
    MobBonus[WR12][BP256] = 1;
    MobBonus[WR12][BN256] = 1;
    MobBonus[WR12][BB256] = 1;
    MobBonus[WR12][BR256] = 1;
    MobBonus[WR12][BQ256] = 4;
    
    MobBonus[WQ12][BP256] = 1;
    MobBonus[WQ12][BN256] = 1;
    MobBonus[WQ12][BB256] = 1;
    MobBonus[WQ12][BR256] = 1;
    MobBonus[WQ12][BQ256] = 1;

    //
    
    MobBonus[BP12][WP256] = 1;
    MobBonus[BP12][WN256] = 2;
    MobBonus[BP12][WB256] = 2;
    MobBonus[BP12][WR256] = 4;
    MobBonus[BP12][WQ256] = 8;
    
    MobBonus[BN12][WP256] = 1;
    MobBonus[BN12][WN256] = 1;
    MobBonus[BN12][WB256] = 1;
    MobBonus[BN12][WR256] = 2;
    MobBonus[BN12][WQ256] = 6;
    
    MobBonus[BB12][WP256] = 1;
    MobBonus[BB12][WN256] = 1;
    MobBonus[BB12][WB256] = 1;
    MobBonus[BB12][WR256] = 2;
    MobBonus[BB12][WQ256] = 6;
    
    MobBonus[BR12][WP256] = 1;
    MobBonus[BR12][WN256] = 1;
    MobBonus[BR12][WB256] = 1;
    MobBonus[BR12][WR256] = 1;
    MobBonus[BR12][WQ256] = 4;
    
    MobBonus[BQ12][WP256] = 1;
    MobBonus[BQ12][WN256] = 1;
    MobBonus[BQ12][WB256] = 1;
    MobBonus[BQ12][WR256] = 1;
    MobBonus[BQ12][WQ256] = 1;

#else

    MobBonus[WN12][PieceNone256] = 1; //
    MobBonus[BN12][PieceNone256] = 1; //

    MobBonus[WP12][BP256] = 1;
    MobBonus[WP12][BN256] = 1;
    MobBonus[WP12][BB256] = 1;
    MobBonus[WP12][BR256] = 1;
    MobBonus[WP12][BQ256] = 1;
    
    MobBonus[WN12][BP256] = 1;
    MobBonus[WN12][BN256] = 1;
    MobBonus[WN12][BB256] = 1;
    MobBonus[WN12][BR256] = 1;
    MobBonus[WN12][BQ256] = 1;
    
    MobBonus[WB12][BP256] = 1;
    MobBonus[WB12][BN256] = 1;
    MobBonus[WB12][BB256] = 1;
    MobBonus[WB12][BR256] = 1;
    MobBonus[WB12][BQ256] = 1;
    
    MobBonus[WR12][BP256] = 1;
    MobBonus[WR12][BN256] = 1;
    MobBonus[WR12][BB256] = 1;
    MobBonus[WR12][BR256] = 1;
    MobBonus[WR12][BQ256] = 1;
    
    MobBonus[WQ12][BP256] = 1;
    MobBonus[WQ12][BN256] = 1;
    MobBonus[WQ12][BB256] = 1;
    MobBonus[WQ12][BR256] = 1;
    MobBonus[WQ12][BQ256] = 1;

    //
    
    MobBonus[BP12][WP256] = 1;
    MobBonus[BP12][WN256] = 1;
    MobBonus[BP12][WB256] = 1;
    MobBonus[BP12][WR256] = 1;
    MobBonus[BP12][WQ256] = 1;
    
    MobBonus[BN12][WP256] = 1;
    MobBonus[BN12][WN256] = 1;
    MobBonus[BN12][WB256] = 1;
    MobBonus[BN12][WR256] = 1;
    MobBonus[BN12][WQ256] = 1;
    
    MobBonus[BB12][WP256] = 1;
    MobBonus[BB12][WN256] = 1;
    MobBonus[BB12][WB256] = 1;
    MobBonus[BB12][WR256] = 1;
    MobBonus[BB12][WQ256] = 1;
    
    MobBonus[BR12][WP256] = 1;
    MobBonus[BR12][WN256] = 1;
    MobBonus[BR12][WB256] = 1;
    MobBonus[BR12][WR256] = 1;
    MobBonus[BR12][WQ256] = 1;
    
    MobBonus[BQ12][WP256] = 1;
    MobBonus[BQ12][WN256] = 1;
    MobBonus[BQ12][WB256] = 1;
    MobBonus[BQ12][WR256] = 1;
    MobBonus[BQ12][WQ256] = 1;

#endif

}

template <side_t Side>
int piece_on_semiopen(const Position& pos, int orig)
{
    constexpr u8 mpawn = make_pawn(Side);
    constexpr u8 opawn = flip_pawn(mpawn);
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
Value eval_knight(const Position& pos, int orig)
{
    Value val;

    val.mg += ValueKnight[PhaseMg];
    val.eg += ValueKnight[PhaseEg];
    val.mg += PSQT[PhaseMg][WN12 + Side][orig];
    val.eg += PSQT[PhaseEg][WN12 + Side][orig];

    constexpr u8 mpawn  = make_pawn(Side);
    int pincr     = pawn_incr(Side);
    int outpost   = minor_on_outpost<Side>(pos, orig);

    val += KnightOutpostBonus * outpost;

    int mob = KnightMobInit;
        
    for (auto inc : KnightIncrs) {
        int sq = orig + inc;

        mob += MobBonus[WN12 + Side][pos[sq]];
    }

    val += Value { KnightMobFactor.mg * mob, KnightMobFactor.eg * mob };
  
    {
        int pcount = pos.pawns(Side);
        constexpr int pbonus[9] = { -5, -4, -3, -2, -1, 0, 1, 2, 3 };

        int bonus = pbonus[pcount];

        val += Value {
            KnightPawnBonus.mg * bonus, 
            KnightPawnBonus.eg * bonus
        };
    }

    if (pos[orig + pincr] == mpawn)
        val += KnightBehindPawnBonus;

    return val;
}

template <side_t Side>
Value eval_bishop(const Position& pos, int orig)
{
    Value val;
    
    val.mg += ValueBishop[PhaseMg];
    val.eg += ValueBishop[PhaseEg];
    val.mg += PSQT[PhaseMg][WB12 + Side][orig];
    val.eg += PSQT[PhaseEg][WB12 + Side][orig];

    constexpr u8 mpawn  = make_pawn(Side);
    constexpr u8 opawn  = make_pawn(flip_side(Side));
    int pincr     = pawn_incr(Side);

    int outpost = minor_on_outpost<Side>(pos, orig);

    val += BishopOutpostBonus * outpost;

    if (Side == White) {
        if (orig == C1 && pos[D2] == mpawn && pos[D3] != PieceNone256)
            val -= BishopBlockedPenalty;
        else if (orig == F1 && pos[E2] == mpawn && pos[E3] != PieceNone256)
            val -= BishopBlockedPenalty;

        if (   (orig == A7 && pos[B6] == opawn)
            || (orig == B8 && pos[C7] == opawn)
            || (orig == H7 && pos[G6] == opawn)
            || (orig == G8 && pos[F7] == opawn)
            || (orig == A6 && pos[B5] == opawn)
            || (orig == H6 && pos[G5] == opawn))
            val -= BishopTrappedPenalty;
    }
    else {
        if (orig == C8 && pos[D7] == mpawn && pos[D6] != PieceNone256)
            val -= BishopBlockedPenalty;
        else if (orig == F8 && pos[E7] == mpawn && pos[E6] != PieceNone256)
            val -= BishopBlockedPenalty;

        if (   (orig == A2 && pos[B3] == opawn)
            || (orig == B1 && pos[C2] == opawn)
            || (orig == H2 && pos[G3] == opawn)
            || (orig == G1 && pos[F2] == opawn)
            || (orig == A3 && pos[B4] == opawn)
            || (orig == H3 && pos[G4] == opawn))
            val -= BishopTrappedPenalty;
    }

    int mob = BishopMobInit;
        
    for (int inc : BishopIncrs) {
        int sq;

        for (sq = orig + inc; pos[sq] == PieceNone256; sq += inc) ++mob;

        mob += MobBonus[WB12 + Side][pos[sq]];
    }

    val += Value { BishopMobFactor.mg * mob, BishopMobFactor.eg * mob };
    
    if (pos[orig + pincr] == mpawn)
        val += BishopBehindPawnBonus;

    return val;
}

template <side_t Side>
Value eval_rook(const Position& pos, int orig)
{
    Value val;
    
    val.mg += ValueRook[PhaseMg];
    val.eg += ValueRook[PhaseEg];
    val.mg += PSQT[PhaseMg][WR12 + Side][orig];
    val.eg += PSQT[PhaseEg][WR12 + Side][orig];
    
    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);
    constexpr u8 opawn  = make_pawn(oside);
    int mksq      = pos.king_sq(mside);
    int oksq      = pos.king_sq(oside);
    
    int rfile     = sq88_file(orig);
    int rrank     = sq88_rank(orig, mside);

    int kfile     = sq88_file(oksq);
    int kfdiff    = abs(rfile - kfile);
    int krank     = sq88_rank(oksq, mside);
    int so        = piece_on_semiopen<Side>(pos, orig);
    
    if (rrank == Rank7) {
        if (krank == Rank8)
            val += RookSeventhRankBonus;
        else {
            int f = mside == White ? A7 : A2;
            int t = mside == White ? H7 : H2;

            int op = 0;

            for (int sq = f; sq <= t; sq++)
                op += pos[sq] == opawn;

            if (op) val += RookSeventhRankBonus;
        }
    }

    if (so == FileClosed)
        val -= RookClosedPenalty;
    else if (so == FileSemi) {
        if (kfdiff == 0)
            val += RookSemiSameKingBonus;
        else if (kfdiff == 1)
            val += RookSemiAdjKingBonus;
        else
            val += RookSemiBonus;
    }
    else if (so == FileOpen) {
        if (kfdiff == 0)
            val += RookOpenSameKingBonus;
        else if (kfdiff == 1)
            val += RookOpenAdjKingBonus;
        else
            val += RookOpenBonus;
    }

    if (mside == White) {
        if ((orig == A1 || orig == A2 || orig == B1) && (mksq == B1 || mksq == C1))
            val -= RookBlockedPenalty;
        else if ((orig == H1 || orig == H2 || orig == G1) && (mksq == F1 || mksq == G1))
            val -= RookBlockedPenalty;
    }
    else {
        if ((orig == A8 || orig == A7 || orig == B8) && (mksq == B8 || mksq == C8))
            val -= RookBlockedPenalty;
        else if ((orig == H8 || orig == H7 || orig == G8) && (mksq == F8 || mksq == G8))
            val -= RookBlockedPenalty;
    }

    int mob = RookMobInit;

    for (int inc : RookIncrs) {
        int sq;

        for (sq = orig + inc; pos[sq] == PieceNone256; sq += inc) ++mob;

        mob += MobBonus[WR12 + mside][pos[sq]];
    }

    val += Value { RookMobFactor.mg * mob, RookMobFactor.eg * mob };

    {
        int pcount = pos.pawns(mside);
        constexpr int pbonus[9] = { 5, 4, 3, 2, 1, 0, -1, -2, -3 };

        int bonus = pbonus[pcount];

        val += Value {
            RookPawnBonus.mg * bonus,
            RookPawnBonus.eg * bonus 
        };
    }

    return val;
}

template <side_t Side>
Value eval_queen(const Position& pos, int orig)
{
    Value val;
    
    val.mg += ValueQueen[PhaseMg];
    val.eg += ValueQueen[PhaseEg];
    val.mg += PSQT[PhaseMg][WQ12 + Side][orig];
    val.eg += PSQT[PhaseEg][WQ12 + Side][orig];

    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);
    constexpr u8 opawn  = make_pawn(oside);

    int ksq       = pos.king_sq(oside);
    int qrank     = sq88_rank(orig, mside);
    int krank     = sq88_rank(ksq, mside);

    if (qrank == Rank7) {
        if (krank == Rank8)
            val += QueenSeventhRankBonus;
        else {
            int f = mside == White ? A7 : A2;
            int t = mside == White ? H7 : H2;

            int op = 0;

            for (int sq = f; sq <= t; sq++)
                op += pos[sq] == opawn;

            if (op) val += QueenSeventhRankBonus;
        }
    }

    int mob = QueenMobInit;
    
    for (int inc : QueenIncrs) {
        int sq;

        for (sq = orig + inc; pos[sq] == PieceNone256; sq += inc) ++mob;

        mob += MobBonus[WQ12 + mside][pos[sq]];
    }

    val += Value { QueenMobFactor.mg * mob, QueenMobFactor.eg * mob };

    return val;
}

template <side_t Side>
Value eval_shelter(const Position& pos)
{
    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);

    constexpr u8 oflag = make_flag(oside);
    constexpr u8 mpawn = make_pawn(mside);

    constexpr int pincr = pawn_incr(mside);

    int ksq = pos.king_sq(mside);

    int penalty = 0;
    
    for (int offset = -1; offset <= 1; offset++) {
        
        int mul = offset ? 1 : 2;
        
        int p = -36;

        for (int sq = ksq + pincr + offset; sq88_is_ok(sq); sq += pincr) {
            u8 piece = pos[sq];

            if (piece & oflag)
                break;
            
            else if (piece == mpawn) {
                int rank = sq88_rank(sq, mside);
                int dist = Rank8 - rank;

                p += dist * dist;

                break;
            }
        }

        penalty += mul * p;
    }

    if (penalty == 0) penalty = -11;

    return Value { penalty, 0 };
}

template <side_t Side>
Value eval_storm(const Position& pos, int ksq)
{
    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);
    constexpr u8 opawn  = make_pawn(oside);
    constexpr int pincr = pawn_incr(mside);

    int penalty = 0;
    
    for (int offset = -1; offset <= 1; offset++) {

        int rank = Rank1;
        
        for (int sq = ksq + pincr + offset; sq88_is_ok(sq); sq += pincr) {
            if (pos[sq] == opawn) {
                rank = sq88_rank(sq, mside);
                break;
            }
        }

        constexpr int bonus[8] = { 0, 0, -60, -30, -10, 0, 0, 0 };

        penalty += bonus[rank];
    }
    
    return Value { penalty, 0 };
}

template <side_t Side>
Value eval_mate(const Position& pos)
{
    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);

    int mksq      = pos.king_sq(mside);
    int oksq      = pos.king_sq(oside);

    Value val;

    if (pos.pawns(mside) || pos.pieces(oside))
        return val;

    if (pos.minors(mside) < 2 && pos.majors(mside) == 0)
        return val;

    int kdist = sq88_dist(mksq, oksq);
    int edist = sq88_dist_edge(oksq);
    int cdist = sq88_dist_corner(oksq);

    assert(kdist >= 0 && kdist <= 7);
    assert(edist >= 0 && edist <= 3);
    assert(cdist >= 0 && cdist <= 3);

    int penalty = 5 * kdist + 5 * cdist + 20 * edist;

    return Value { 0, -penalty };
}

Value eval_complexity(const Position& pos, const Value& val)
{
    int eg        = val.eg;
    int sign      = (eg > 0) - (eg < 0);
    int pawns     = pos.pawns();
    int minors    = pos.minors();
    int majors    = pos.majors();
    bool pflanked = pawns_on_file_a(pos) && pawns_on_file_h(pos);

    Value c = ComplexityPawnsTotal      * pawns
            + ComplexityPawnsFlanked    * pflanked
            + ComplexityPawnsEndgame    * !(minors || majors)
            + ComplexityAdjustment;

    int v = sign * max(c.eg, -abs(eg));

    return Value { 0, v };
}

int eval_scale_factor(const Position& pos, const Value& val)
{
    int wp, wn, wlb, wdb, wr, wq;
    int bp, bn, blb, bdb, br, bq;

    pos.counts(wp, wn, wlb, wdb, wr, wq, bp, bn, blb, bdb, br, bq);

    // Opposite colored bishops

    if (wlb + wdb == 1 && blb + bdb == 1 && wlb + blb == 1) {
        if (!(wr + wq + br + bq) && wn == 1 && bn == 1)
            return ScaleOcbOneKnight.eg;

        if (!(wn + wq + bn + bq) && wr == 1 && br == 1)
            return ScaleOcbOneRook.eg;

        if (!(wn + wr + wq + bn + br + bq))
            return ScaleOcbBishopsOnly.eg;
    }
    
    int lo_pawns;
    int lo_pieces;
    int hi_pawns;
    int hi_minors;
    int hi_total;

    if (val.eg < 0) {
        lo_pawns = wp;
        lo_pieces = wn + wlb + wdb + wr;
        hi_pawns = bp;
        hi_minors = bn + blb + bdb;
        hi_total = bp + bn + blb + bdb + br + bq;
    }
    else {
        lo_pawns = bp;
        lo_pieces = bn + blb + bdb + br;
        hi_pawns = wp;
        hi_minors = wn + wlb + wdb;
        hi_total = wp + wn + wlb + wdb + wr + wq;
    }

    int wpieces = wn + wlb + wdb + wr;
    int bpieces = bn + blb + bdb + br;
    int tpieces = wpieces + bpieces;

    if (wq + bq == 1 && tpieces > 1 && tpieces == lo_pieces)
        return ScaleLoneQueen.eg;

    if (hi_minors && hi_total == 1)
        return ScaleDraw.eg;

    if (wq + bq == 0 && wpieces < 2 && bpieces < 2 && hi_pawns - lo_pawns > 2)
        return ScaleLargePawnAdv.eg;

    return min(ScaleNormal.eg, ScaleM.eg * hi_pawns + ScaleB.eg);
}

template <side_t Side>
Value eval_king(const Position& pos)
{
    constexpr side_t mside =           Side;
    constexpr side_t oside = flip_side(Side);

    int ksq = pos.king_sq(mside);

    Value val;
    
    val.mg += ValueKing[PhaseMg];
    val.eg += ValueKing[PhaseEg];
    val.mg += PSQT[PhaseMg][WK12 + mside][ksq];
    val.eg += PSQT[PhaseEg][WK12 + mside][ksq];

    if (!pos.queens(Black - mside) || !pos.pieces(Black - mside))
        return val;

    bitset<192> kzone;

    pos.king_zone(kzone, mside);

    int natt = 0;
    int batt = 0;
    int ratt = 0;
    int qatt = 0;

    int dest;

    for (int orig : pos.plist(WN12 + oside)) {
        for (int inc : KnightIncrs) {
            dest = orig + inc;

            if (kzone.test(36 + dest)) {
                ++natt;
                break;
            }
        }
    }
    
    for (int orig : pos.plist(WB12 + oside)) {
        for (int inc : BishopIncrs) {
            for (dest = orig + inc; pos[dest] == PieceNone256; dest += inc) {
                if (kzone.test(36 + dest)) {
                    ++batt;
                    goto next_bishop;
                }
            }

            if (kzone.test(36 + dest)) {
                ++batt;
                break;
            }
        }
next_bishop:
        continue;
    }
    
    for (int orig : pos.plist(WR12 + oside)) {
        for (int inc : RookIncrs) {
            for (dest = orig + inc; pos[dest] == PieceNone256; dest += inc) {
                if (kzone.test(36 + dest)) {
                    ++ratt;
                    goto next_rook;
                }
            }

            if (kzone.test(36 + dest)) {
                ++ratt;
                break;
            }
        }
next_rook:
        continue;
    }
    
    for (int orig : pos.plist(WQ12 + oside)) {
        for (int inc : QueenIncrs) {
            for (dest = orig + inc; pos[dest] == PieceNone256; dest += inc) {
                if (kzone.test(36 + dest)) {
                    ++qatt;
                    goto next_queen;
                }
            }

            if (kzone.test(36 + dest)) {
                ++qatt;
                break;
            }
        }
next_queen:
        continue;
    }

    int att_weight[8] = { 0, 0, 50, 75, 88, 94, 97, 99 };
    int att_count = min(7, qatt + ratt + batt + natt);
    int att_value = 4 * qatt + 2 * ratt + batt + natt;
   
    int e = att_weight[att_count] * 25 * att_value / 100;
    
    val += Value { -e, 0 };

    int so = piece_on_semiopen<Side>(pos, ksq);

    if (so == FileSemi)
        val -= KingSemiPenalty;
    else if (so == FileOpen)
        val -= KingOpenPenalty;

    val += eval_shelter<Side>(pos);
    val += eval_storm<Side>(pos, ksq);

    val.mg = KingSafetyFactor.mg * val.mg / 100;
    val.eg = KingSafetyFactor.eg * val.eg / 100;

    return val;
}

template <side_t Side>
int minor_on_outpost(const Position& pos, int orig)
{
    constexpr int pincr = pawn_incr(Side);
    constexpr u8 mpawn  = make_pawn(Side);
    constexpr u8 opawn  = flip_pawn(mpawn);
    int sqref     = Side == White ? orig : sq88_reflect(orig);
    int bonus     = minor_outpost_bonus[sqref];

    if (bonus == 0) return 0;

    int mp = 0;

    if (pos[orig - pincr - 1] == mpawn) ++mp;
    if (pos[orig - pincr + 1] == mpawn) ++mp;

    if (mp == 0) return 0;

    int op = 0;
    
    for (int sq = orig + pincr - 1; sq >= A2 && sq <= H7; sq += pincr) op += pos[sq] == opawn;
    for (int sq = orig + pincr + 1; sq >= A2 && sq <= H7; sq += pincr) op += pos[sq] == opawn;

    return op ? 0 : bonus * mp;
}

template <side_t Side>
Value eval_pieces(const Position& pos)
{
    Value val;

    for (int orig : pos.plist(WN12 + Side)) val += eval_knight<Side>(pos, orig);
    for (int orig : pos.plist(WB12 + Side)) val += eval_bishop<Side>(pos, orig);
    for (int orig : pos.plist(WR12 + Side)) val += eval_rook  <Side>(pos, orig);
    for (int orig : pos.plist(WQ12 + Side)) val += eval_queen <Side>(pos, orig);
    
    if (pos.bishop_pair<Side>()) val += BishopPairBonus;
  
    return val;
}

int pawns_on_file_a(const Position& pos)
{
    int count = 0;

    for (int sq = A2; sq <= A7; sq += 16)
        count += is_pawn(pos[sq]);

    return count;
}

int pawns_on_file_h(const Position& pos)
{
    int count = 0;

    for (int sq = H2; sq <= H7; sq += 16)
        count += is_pawn(pos[sq]);

    return count;
}

int eval_internal(const Position& pos, int alpha, int beta)
{
    //assert(!pos.in_check());

    EvalEntry eentry;

    if (ehtable.get(pos.key(), eentry))
        return eentry.score + TempoBonus;
    
    Value val;

    if (pos.pawns())
        phtable.prefetch(pos.pawn_key());

    // Major and minor pieces
    
    val += eval_pieces<White>(pos);
    val -= eval_pieces<Black>(pos);
    
    // Evaluation of pawns is done separately because all pawns share a hash
    // entry since the pawn key is formed from all pawns
   
    PawnEntry pentry;

    // TODO maybe after lazy eval instead?
    
    if (pos.pawns()) {
        val += eval_pawns(pos, pentry);
        val += eval_passers(pos, pentry);
    }

    if (UseLazyEval) {
        if (pos.pieces(White) > 1 && pos.pieces(Black) > 1) {
            int leval = val.lerp(pos.phase());
            
            if (pos.side() == Black) leval = -leval;

            if (leval + LazyMargin <= alpha) 
                return leval;
            if (leval - LazyMargin >= beta) 
                return leval;
        }
    }

    val += eval_king<White>(pos);
    val -= eval_king<Black>(pos);
   
    // TODO maybe do this before lazy eval?
    val += eval_complexity(pos, val);

    int sfactor = eval_scale_factor(pos, val);
    
    //val += eval_mate<White>(pos);
    //val -= eval_mate<Black>(pos);

    int score = val.lerp(pos.phase(), sfactor);

    if (pos.side() == Black) score = -score;

    eentry.score = score;

    ehtable.set(pos.key(), eentry);
    
    return score + TempoBonus;
}

int eval(const Position& pos, int alpha, int beta)
{
    Timer timer(Profile >= ProfileLevel::Medium);

    int e = eval_internal(pos, alpha, beta);
    
    if (timer.stop()) {
        gstats.time_eval_ns += timer.elapsed_time<Timer::Nano>();
        gstats.cycles_eval += timer.elapsed_cycles();
        gstats.evals_count++;
    }

    return e;
}

// Begin tuner

vector<Position> gposs;

double sigmoid(const Position& pos, int eval)
{
    if (pos.side() == Black) eval = -eval;

    double K = 2.47;
    double e = -K * eval / 400;

    return 1 / (1 + exp(e));
}

double eval_error(const Position& pos)
{
    int    ev     = eval(pos, -ScoreMate, ScoreMate);
    double error  = pow(pos.outcome() - sigmoid(pos, ev), 2);

    return error;
}

double eval_error(const vector<Position>& v)
{
    double ss = 0;

    #pragma omp parallel num_threads(11)
    {
        double ss_part      = 0;
        size_t thread_id    = omp_get_thread_num();
        size_t thread_num   = omp_get_num_threads();

        for (size_t i = thread_id; i < v.size(); i += thread_num)
            ss_part += eval_error(v[i]);

        #pragma omp critical
        {
            ss += ss_part;
        }
    }

    return ss / v.size();
}

void eval_fread(vector<Position>& v, string filename, size_t skip)
{
    string line;

    ifstream ifs(filename);

    uniform_int_distribution<size_t> dist(1, skip);

    while (getline(ifs, line)) {
        if (dist(mt) != 1) continue;

        assert(!line.empty());

        size_t p0 = line.find('[');
        size_t p1 = line.find(']');

        assert(p0 != string::npos);
        assert(p1 != string::npos);

        string fen = line.substr(0, p0);
        string wdl = line.substr(p0 + 1, p1 - p0 - 1);
        double score = stod(wdl);

        v.emplace_back(fen, score);
    }
}

struct Term {
    string n_;
    Value* v_;
    int mg_min_;
    int mg_max_;
    int eg_min_;
    int eg_max_;
    double mg_sd_;
    double eg_sd_;

    Term(string n, Value* v, int mg_min, int mg_max, int eg_min, int eg_max) : n_(n), v_(v)
    {
        mg_min_ = mg_min;
        mg_max_ = mg_max;
        eg_min_ = eg_min;
        eg_max_ = eg_max;

        set_sds();
    }
    
    Term(string n, Value* v, int range = 10) : n_(n), v_(v)
    {
        mg_min_ = v_->mg - range;
        mg_max_ = v_->mg + range;
        eg_min_ = v_->eg - range;
        eg_max_ = v_->eg + range;

        set_sds();
    }

    void perturb()
    {
        uniform_int_distribution<int> mg_dist(mg_min_, mg_max_);
        uniform_int_distribution<int> eg_dist(eg_min_, eg_max_);

        v_->mg = mg_dist(mt);
        v_->eg = eg_dist(mt);
    }

    void set_sds()
    {
        mg_sd_ = sqrt(pow(mg_max_ - mg_min_, 2) / 12.0);
        eg_sd_ = sqrt(pow(eg_max_ - eg_min_, 2) / 12.0);
    }

    string to_string() const
    {
        ostringstream oss;

        oss << "Value " << n_ << " { " << v_->mg << ", " << v_->eg << " };";

        return oss.str();
    }
};

vector<Term> gterms;
    
struct Terms {
    vector<Value> values_;
    double error_ = 1;

    Terms(vector<Term> terms)
    {
        for (Term t : terms) values_.push_back(*t.v_);
    }

    void run()
    {
        for (size_t i = 0; i < gterms.size(); i++)
            *gterms[i].v_ = values_[i];

        eval_init();

        error_ = eval_error(gposs);
    }

    string to_string() const
    {
        ostringstream oss;

        for (size_t i = 0; i < gterms.size(); i++)
            oss << gterms[i].to_string() << endl;

        return oss.str();
    }

    bool operator<(const Terms& t) const
    {
        return error_ < t.error_;
    }

    void perturb()
    {
        for (size_t i = 0; i < values_.size(); i++) {
            int mg_min = gterms[i].mg_min_;
            int mg_max = gterms[i].mg_max_;
            int eg_min = gterms[i].eg_min_;
            int eg_max = gterms[i].eg_max_;
        
            uniform_int_distribution<int> mg_dist(mg_min, mg_max);
            uniform_int_distribution<int> eg_dist(eg_min, eg_max);

            values_[i].mg = mg_dist(mt);
            values_[i].eg = eg_dist(mt);
        }
    }
};

struct Pool {
    const double trials;

    vector<Terms> terms_;

    vector<double> curr_means_mg_;
    vector<double> curr_means_eg_;
    vector<double> prev_means_mg_;
    vector<double> prev_means_eg_;

    const size_t N  = 100;
    const size_t Ne = 10;
    
    double remaining;

    Pool(Terms terms, size_t t) : trials(t), remaining(t)
    {
        for (size_t i = 0; i < N; i++)
            terms_.push_back(terms);

        for (size_t i = 0; i < terms_[0].values_.size(); i++) {
            curr_means_mg_.push_back(terms_[0].values_[i].mg);
            curr_means_eg_.push_back(terms_[0].values_[i].eg);
        }
    }

    void run()
    {
        for (Terms& t : terms_) t.run();
        
        std::sort(terms_.begin(), terms_.end());

        calc_means();

        remaining--;
    }

    void calc_means()
    {
        prev_means_mg_ = curr_means_mg_;
        prev_means_eg_ = curr_means_eg_;

        curr_means_mg_.clear();
        curr_means_eg_.clear();

        for (size_t i = 0; i < terms_[0].values_.size(); i++) {
            double sum_mg = 0;
            double sum_eg = 0;

            for (size_t j = 0; j < Ne; j++) {
                sum_mg += terms_[j].values_[i].mg;
                sum_eg += terms_[j].values_[i].eg;
            }

            curr_means_mg_.push_back(sum_mg / Ne);
            curr_means_eg_.push_back(sum_eg / Ne);
        }
    }

    double error() const
    {
        double sum = 0;

        for (size_t i = 0; i < N; i++)
            sum += terms_[i].error_;

        return sum / N;
    }

    double error_elite() const
    {
        double sum = 0;

        for (size_t i = 0; i < Ne; i++)
            sum += terms_[i].error_;

        return sum / Ne;
    }

    double decay() const { return remaining / trials; }

    void update()
    {
        for (size_t i = 0; i < terms_[0].values_.size(); i++) {
            double cmg_v    = curr_means_mg_[i];
            int mg_min      = gterms[i].mg_min_;
            int mg_max      = gterms[i].mg_max_;
            double ceg_v    = curr_means_eg_[i];
            int eg_min      = gterms[i].eg_min_;
            int eg_max      = gterms[i].eg_max_;

            double mg_sd = (1 + gterms[i].mg_sd_) * decay();
            double eg_sd = (1 + gterms[i].eg_sd_) * decay();

            normal_distribution<> dist_mg { cmg_v, mg_sd };
            normal_distribution<> dist_eg { ceg_v, eg_sd };

            for (size_t j = 0; j < N; j++) {
                double mg = dist_mg(mt);
                double pmg_v = prev_means_mg_[i];
                double eg = dist_eg(mt);
                double peg_v = prev_means_eg_[i];

                double alpha = 0.2;

                mg = alpha * mg + (1 - alpha) * pmg_v;
                eg = alpha * eg + (1 - alpha) * peg_v;

                mg = clamp(round(mg), (double)mg_min, (double)mg_max);
                eg = clamp(round(eg), (double)eg_min, (double)eg_max);

                terms_[j].values_[i].mg = (int)mg;
                terms_[j].values_[i].eg = (int)eg;
            }
        }
    }

    bool convergence() const
    {
        for (size_t i = 0; i < curr_means_mg_.size(); i++) {
            double cmm = curr_means_mg_[i];
            double cme = curr_means_eg_[i];
            double pmm = prev_means_mg_[i];
            double pme = prev_means_eg_[i];

            if (abs(cmm - pmm) > 0.4) return false;
            if (abs(cme - pme) > 0.4) return false;
        }

        return true;
    }

private:

    void perturb() { for (Terms& t : terms_) t.perturb(); }
};

void eval_tune(int argc, char* argv[])
{
    assert(argc >= 1);

    string filename = argv[0];
        
    if (argc >= 2 && string(argv[1]) == "-c") {
        eval_fread(gposs, filename, 1);
        cout << eval_error(gposs) << endl;
        return;
    }

    // Leave these alone

    gterms.push_back(Term("KnightMobFactor",    &KnightMobFactor,   1, 3, 6, 8));
    gterms.push_back(Term("KnightPawnBonus",    &KnightPawnBonus,   1, 3, 5, 7));
    gterms.push_back(Term("BishopMobFactor",    &BishopMobFactor,   2, 4, 5, 7));
    gterms.push_back(Term("RookMobFactor",      &RookMobFactor,     0, 2, 4, 6));
    gterms.push_back(Term("RookPawnBonus",      &RookPawnBonus,     4, 6, 1, 3));
    gterms.push_back(Term("QueenMobFactor",     &QueenMobFactor,    0, 2, 5, 7));

    // Need little attention

    gterms.push_back(Term("ValuePawn",      &ValuePawn,     5));
    gterms.push_back(Term("ValueKnight",    &ValueKnight,   5));
    gterms.push_back(Term("ValueBishop",    &ValueBishop,   5));
    gterms.push_back(Term("ValueRook",      &ValueRook,     5));
    gterms.push_back(Term("ValueQueen",     &ValueQueen,    5));
    gterms.push_back(Term("ValueKing",      &ValueKing,     0));

    // Almost irrelevant
    
    gterms.push_back(Term("BishopBlockedPenalty",   &BishopBlockedPenalty));    // T2 50
    gterms.push_back(Term("BishopTrappedPenalty",   &BishopTrappedPenalty));    // T2 100
    gterms.push_back(Term("RookBlockedPenalty",     &RookBlockedPenalty));      // T2 50

    // The rest need work
    
    gterms.push_back(Term("PsqtPFactor", &PsqtPFactor,  37,  47, 100, 100));
    gterms.push_back(Term("PsqtNFactor", &PsqtNFactor));
    gterms.push_back(Term("PsqtBFactor", &PsqtBFactor));
    gterms.push_back(Term("PsqtRFactor", &PsqtRFactor));
    gterms.push_back(Term("PsqtQFactor", &PsqtQFactor, 100, 100, 120, 130));
    gterms.push_back(Term("PsqtKFactor", &PsqtKFactor));

    gterms.push_back(Term("PawnBackOpenPenalty", &PawnBackOpenPenalty)); // T2 16, 10
    gterms.push_back(Term("PawnBackwardPenalty", &PawnBackwardPenalty)); // T2 8, 10
    gterms.push_back(Term("PawnCandidateBase", &PawnCandidateBase)); // T2 5, 10
    gterms.push_back(Term("PawnCandidateFactor", &PawnCandidateFactor)); // T2 50, 100
    gterms.push_back(Term("PawnDoubledPenalty", &PawnDoubledPenalty)); // T2 10, 20
    gterms.push_back(Term("PawnIsoOpenPenalty", &PawnIsoOpenPenalty)); // T2 20, 20
    gterms.push_back(Term("PawnIsolatedPenalty", &PawnIsolatedPenalty)); // T2 10, 20
    gterms.push_back(Term("PawnPassedBase", &PawnPassedBase)); // T2 10, 20
    gterms.push_back(Term("PawnPassedFactor1", &PawnPassedFactor1)); // T2 20, 60
    gterms.push_back(Term("PawnPassedFactor2", &PawnPassedFactor2)); // T2 60, 120
    
    gterms.push_back(Term("KnightBehindPawnBonus", &KnightBehindPawnBonus));
    gterms.push_back(Term("KnightOutpostBonus", &KnightOutpostBonus));

    gterms.push_back(Term("BishopBehindPawnBonus", &BishopBehindPawnBonus));
    gterms.push_back(Term("BishopPairBonus", &BishopPairBonus)); // T2 50
    gterms.push_back(Term("BishopOutpostBonus", &BishopOutpostBonus));

    gterms.push_back(Term("RookOpenBonus", &RookOpenBonus));
    gterms.push_back(Term("RookSemiBonus", &RookSemiBonus));
    gterms.push_back(Term("RookClosedPenalty", &RookClosedPenalty));
    gterms.push_back(Term("RookSemiAdjKingBonus", &RookSemiAdjKingBonus));
    gterms.push_back(Term("RookSemiSameKingBonus", &RookSemiSameKingBonus));
    gterms.push_back(Term("RookOpenAdjKingBonus", &RookOpenAdjKingBonus));
    gterms.push_back(Term("RookOpenSameKingBonus", &RookOpenSameKingBonus));

    gterms.push_back(Term("RookSeventhRankBonus", &RookSeventhRankBonus));
    gterms.push_back(Term("QueenSeventhRankBonus", &QueenSeventhRankBonus));

    gterms.push_back(Term("KingOpenPenalty", &KingOpenPenalty));
    gterms.push_back(Term("KingSemiPenalty", &KingSemiPenalty));

    gterms.push_back(Term("KingSafetyFactor", &KingSafetyFactor, 79, 89, 100, 100));
    
    gterms.push_back(Term("ComplexityPawnsTotal",   &ComplexityPawnsTotal,   0, 0,  -1,   9));
    gterms.push_back(Term("ComplexityPawnsFlanked", &ComplexityPawnsFlanked, 0, 0,   5,  15));
    gterms.push_back(Term("ComplexityPawnsEndgame", &ComplexityPawnsEndgame, 0, 0,  64,  74));
    gterms.push_back(Term("ComplexityAdjustment",   &ComplexityAdjustment,   0, 0, -30, -20));

    gterms.push_back(Term("ScaleOcbBishopsOnly", &ScaleOcbBishopsOnly, 0, 0,  57,  67));
    gterms.push_back(Term("ScaleOcbOneKnight",   &ScaleOcbOneKnight,   0, 0, 112, 122));
    gterms.push_back(Term("ScaleOcbOneRook",     &ScaleOcbOneRook,     0, 0, 109, 119));
    gterms.push_back(Term("ScaleLoneQueen",      &ScaleLoneQueen,      0, 0, 105, 115));
    gterms.push_back(Term("ScaleLargePawnAdv",   &ScaleLargePawnAdv,   0, 0, 150, 160));
    gterms.push_back(Term("ScaleB",              &ScaleB,              0, 0,  89,  99));
    gterms.push_back(Term("ScaleM",              &ScaleM,              0, 0,  12,  22));

    Terms terms(gterms);

    cout << "Optimizing " << gterms.size() << " terms" << endl;
   
    cout << "Reading tuning file... ";
    eval_fread(gposs, filename, 2);
    cout << "done" << endl;
        
    for (Term& t : gterms) t.perturb();

    size_t trials = 100;
    double error_first = 0;

    Pool pool(terms, trials);

    for (size_t i = 0; i < trials; i++) {

        pool.run();

        if (i == 0) error_first = pool.error_elite();

        cout << i << ',' << pool.error_elite() << ',' << pool.decay() << endl;

        cout << pool.terms_[0].values_[0].mg << ',' << pool.terms_[0].values_[0].eg << endl;

        if ((i + 1) % 10 == 0) cout << terms.to_string() << endl;

        pool.update();
    
        if (i >= 5 && pool.convergence()) break;
    }

    cout << terms.to_string() << endl;
    cout << error_first << endl;
    cout << pool.error_elite() << endl;

    double diff = error_first - pool.error_elite();

    cout << (100.0 * diff / pool.error_elite()) << endl;
}

// End tuner
