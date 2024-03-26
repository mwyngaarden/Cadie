#include <bit>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <vector>
#include <cassert>
#include <cmath>
#include "attack.h"
#include "bb.h"
#include "eval.h"
#include "gen.h"
#include "ht.h"
#include "pawn.h"
#include "search.h"

#ifdef TUNE
#include <nlopt.hpp>
#include <omp.h>
#endif

using namespace std;

random_device rd;
mt19937_64 mt;

enum { FileClosed, FileSemi, FileOpen };

HashTable<EHSizeMBDefault * 1024, EvalEntry> etable;
HashTable<PHSizeMBDefault * 1024, PawnEntry> ptable;

constexpr int cmh_dist_alt[64] = {
    -3, -2, -1,  0,  0, -1, -2, -3,
    -2, -1,  0,  1,  1,  0, -1, -2,
    -1,  0,  1,  2,  2,  1,  0, -1,
     0,  1,  2,  3,  3,  2,  1,  0,
     0,  1,  2,  3,  3,  2,  1,  0,
    -1,  0,  1,  2,  2,  1,  0, -1,
    -2, -1,  0,  1,  1,  0, -1, -2,
    -3, -2, -1,  0,  0, -1, -2, -3
};

int PSQT[2][12][64];

int ValuePawnMg                  =       95;
int ValuePawnEg                  =      125;
int ValueKnightMg                =      198;
int ValueKnightEg                =      546;
int ValueBishopMg                =      209;
int ValueBishopEg                =      566;
int ValueRookMg                  =      254;
int ValueRookEg                  =      935;
int ValueQueenMg                 =      572;
int ValueQueenEg                 =     1939;

int BishopPairBMg                =        9;
int BishopPairBEg                =       98;

int TempoB                       =       12;

int NPSFactorEg                  =        7;
int BPSFactorEg                  =        8;
int RPSFactorEg                  =       17;
int QPSFactorEg                  =       38;
int KPSFactorMg                  =      -10;
int KPSFactorEg                  =       19;
int NRank1Pm                     =      -12;
int BRank1Pm                     =       -6;
int PRank5Eg                     =        6;
int PRank6Eg                     =       55;
int PRank7Eg                     =      123;
int PFileAHBm                    =      -46;
int PFileAHBe                    =       44;
int PFileBGBm                    =      -46;
int PFileBGBe                    =       44;
int PFileCFBm                    =      -49;
int PFileCFBe                    =       47;
int PFileDEBm                    =      -45;
int PFileDEBe                    =       34;
int NRank4Bm                     =       10;
int NRank4Be                     =       25;
int NRank5Bm                     =       14;
int NRank5Be                     =       29;
int NRank6Bm                     =       34;
int NRank6Be                     =       25;
int RRank7Bm                     =       48;
int RRank7Be                     =       19;
int RRank8Bm                     =       42;
int RRank8Be                     =       52;
int QRank7Be                     =      117;
int QRank8Be                     =      155;
int NCornerPm                    =      -79;
int NCornerPe                    =      -56;
int KCornerPm                    =       -7;
int KCornerPe                    =      -45;
int RFileAHBm                    =       -9;
int RFileAHBe                    =       47;
int RFileBGBm                    =       -3;
int RFileBGBe                    =       34;
int RFileCFBm                    =        5;
int RFileCFBe                    =       17;
int RFileDEBm                    =       10;
int RFileDEBe                    =       -8;
int QFileAHBe                    =       44;
int QFileBGBe                    =       13;
int QFileCFBe                    =      -23;
int QFileDEBe                    =      -53;
int KCastleBm                    =        0;
int KCastlePm                    =      -34;

int NMobFactorMg                 =        5;
int NMobFactorEg                 =        9;
int BMobFactorMg                 =        5;
int BMobFactorEg                 =        9;
int RMobFactorMg                 =        3;
int RMobFactorEg                 =        7;
int QMobFactorMg                 =        3;
int QMobFactorEg                 =        4;

int KingSafetyA                  =       29;
int KingSafetyB                  =       17;
int KingSafetyC                  =       22;
int KingSafetyD                  =       -1;

int KingStormP[4][8] = {
    { 0, 0, -21,  -7,   2, 0, 0, 0 },
    { 0, 0, -63, -19,  -4, 0, 0, 0 },
    { 0, 0, -76, -15,   0, 0, 0, 0 },
    { 0, 0, -98, -48,  -9, 0, 0, 0 }
};

int KingShelterP[4][8] = {
    { 0, 0,   2,   0,  -6,  -4, -38, -19 },
    { 0, 0, -19, -28, -34, -14, -16, -34 },
    { 0, 0, -20, -23, -14,  -1,  72, -31 },
    { 0, 0, -31, -32, -33, -46,  -7, -47 }
};

int MinorBehindPawnBonusMg       =        3;
int MinorBehindPawnBonusEg       =       16;
int KnightOutpostBonusMg         =        7;
int KnightOutpostBonusEg         =       11;
int KnightPawnBonusMMg           =       -3;
int KnightPawnBonusMEg           =        5;
int KnightPawnBonusBAg           =        5;
int KnightDistOffsetMg           =        0;
int KnightDistOffsetEg           =       33;
int BishopOutpostBonusMg         =       21;
int BishopOutpostBonusEg         =        1;
int BishopBlockedPm              =      -47;
int BishopTrappedPm              =      -84;
int BishopQueenBonusMg           =        4;
int BishopQueenBonusEg           =       39;
int BishopPawnSquareBonusMg      =       -3;
int BishopPawnSquareBonusEg      =       -5;
int BishopPawnBonusMMg           =        1;
int BishopPawnBonusMEg           =      -12;
int BishopPawnBonusBAg           =        6;
int RookBlockedPm                =      -50;
int RookClosedPm                 =      -18;
int RookClosedPe                 =       23;
int RookOpenAdjKingBonusMg       =       49;
int RookOpenAdjKingBonusEg       =       34;
int RookOpenBonusMg              =        4;
int RookOpenBonusEg              =       43;
int RookOpenSameKingBonusMg      =       91;
int RookOpenSameKingBonusEg      =       24;
int RookRookBonusMg              =        1;
int RookRookBonusEg              =       30;
int RookQueenBonusMg             =        4;
int RookQueenBonusEg             =       53;
int RookPawnBonusMMg             =        4;
int RookPawnBonusMEg             =      -13;
int RookPawnBonusBAg             =        5;
int RookSemiAdjKingBonusMg       =       -1;
int RookSemiAdjKingBonusEg       =       19;
int RookSemiBonusMg              =      -15;
int RookSemiBonusEg              =       45;
int RookSemiSameKingBonusMg      =       28;
int RookSemiSameKingBonusEg      =        7;
int RookSeventhRankBonusMg       =      -38;
int RookSeventhRankBonusEg       =       36;
int QueenSeventhRankBonusMg      =      -47;
int QueenSeventhRankBonusEg      =        2;

int PawnBackOpenPm               =       -9;
int PawnBackOpenPe               =      -18;
int PawnBackwardPm               =       -6;
int PawnBackwardPe               =       -3;
int PawnCandidateBaseMg          =       -2;
int PawnCandidateBaseEg          =       16;
int PawnCandidateFactorMg        =       85;
int PawnCandidateFactorEg        =      -10;
int PawnDoubledPm                =        0;
int PawnDoubledPe                =       -9;
int PawnIsoOpenPm                =       -2;
int PawnIsoOpenPe                =      -23;
int PawnIsolatedPm               =      -16;
int PawnIsolatedPe               =      -29;
int PawnPassedBaseMg             =      -24;
int PawnPassedBaseEg             =       44;
int PawnPassedFactorMg           =      137;
int PawnPassedFactorEg           =       57;
int PawnPassedFactorA            =       67;
int PawnPassedFactorB            =       36;
int PawnPassedFactorC            =      158;
int PawnAttackMg                 =       23;
int PawnAttackEg                 =       15;

int ComplexityPawnsTotal         =        1;
int ComplexityPawnsFlanked       =       23;
int ComplexityPawnsEndgame       =       53;
int ComplexityAdjustment         =      -24;
int ScaleOcbBishopsOnly          =       44;
int ScaleOcbOneKnight            =       99;
int ScaleOcbOneRook              =      112;
int ScaleLoneQueen               =       89;
int ScaleLargePawnAdv            =      159;
int ScaleM                       =       37;
int ScaleB                       =       33;


template <side_t Side> static int piece_on_semiopen (const Position& pos, int orig);
template <side_t Side> static int minor_on_outpost  (const Position& pos, int orig);
template <side_t Side> static Value eval_pieces     (const Position& pos);

static int eval_shelter   (const Position& pos, side_t side, int ksq);
static int eval_storm     (const Position& pos, side_t side, int ksq);
static int eval_complexity(const Position& pos, int score);
static int eval_scale     (const Position& pos, int score);

void eval_init()
{
    int cmhd_mul[2][6] = {
        { 0, 0, 0, 0, 0, KPSFactorMg },
        { 0, NPSFactorEg, BPSFactorEg, RPSFactorEg, QPSFactorEg, KPSFactorEg }
    };
    
    int backrank_p_mg[6] = { 0, NRank1Pm, BRank1Pm, 0, 0, 0 };

    int corner_p[2][6] = {
        { 0, NCornerPm, 0, 0, 0, KCornerPm },
        { 0, NCornerPe, 0, 0, 0, KCornerPe }
    };

    int pawn_rank_eg[8] = { 0, 0, 0, 0, PRank5Eg, PRank6Eg, PRank7Eg, 0 };

    for (int phase : { PhaseMg, PhaseEg }) {
        
        for (int p12 = WP12; p12 < 12; p12++) {

            int side = p12 & 1;
            int type = p12 / 2;

            for (int sq = 0; sq < 64; sq++) {
                int rank = square::rank(sq, side);
                int file = square::file(sq);

                if (type == Pawn && (rank == Rank1 || rank == Rank8)) continue;
                
                int score = 0;

                if (square::corner(sq)) score += corner_p[phase][type];

                // Pawn
                if (type == Pawn) {
                    if (phase == PhaseMg) {
                        if (file == FileA || file == FileH) score += PFileAHBm;
                        if (file == FileB || file == FileG) score += PFileBGBm;
                        if (file == FileC || file == FileF) score += PFileCFBm;
                        if (file == FileD || file == FileE) score += PFileDEBm;
                    }
                    else {
                        if (file == FileA || file == FileH) score += PFileAHBe;
                        if (file == FileB || file == FileG) score += PFileBGBe;
                        if (file == FileC || file == FileF) score += PFileCFBe;
                        if (file == FileD || file == FileE) score += PFileDEBe;

                        score += pawn_rank_eg[rank];
                    }
                }

                // Knight
                if (type == Knight) {
                    if (phase == PhaseMg) {
                        if (file >= FileB && file <= FileG) {
                            if (rank == Rank4) score += NRank4Bm;
                            if (rank == Rank5) score += NRank5Bm;
                            if (rank == Rank6) score += NRank6Bm;
                        }
                    }
                    else {
                        if (file >= FileB && file <= FileG) {
                            if (rank == Rank4) score += NRank4Be;
                            if (rank == Rank5) score += NRank5Be;
                            if (rank == Rank6) score += NRank6Be;
                        }
                    }
                }
               
                // Rook
                if (type == Rook) {
                    if (phase == PhaseMg) {
                        if (file == FileA || file == FileH) score += RFileAHBm;
                        if (file == FileB || file == FileG) score += RFileBGBm;
                        if (file == FileC || file == FileF) score += RFileCFBm;
                        if (file == FileD || file == FileE) score += RFileDEBm;
                        
                        if (rank == Rank7) score += RRank7Bm;
                        if (rank == Rank8) score += RRank8Bm;
                    }
                    else {
                        if (file == FileA || file == FileH) score += RFileAHBe;
                        if (file == FileB || file == FileG) score += RFileBGBe;
                        if (file == FileC || file == FileF) score += RFileCFBe;
                        if (file == FileD || file == FileE) score += RFileDEBe;
                        
                        if (rank == Rank7) score += RRank7Be;
                        if (rank == Rank8) score += RRank8Be;
                    }
                }
                
                // Queen
                if (type == Queen) {
                    if (phase == PhaseEg) {
                        if (file == FileA || file == FileH) score += QFileAHBe;
                        if (file == FileB || file == FileG) score += QFileBGBe;
                        if (file == FileC || file == FileF) score += QFileCFBe;
                        if (file == FileD || file == FileE) score += QFileDEBe;

                        if (rank == Rank7) score += QRank7Be;
                        if (rank == Rank8) score += QRank8Be;
                    }
                }

                // King
                if (type == King) {
                    if (phase == PhaseMg) {
                        if (rank == Rank1) {
                            if (file == FileC) score += KCastleBm;
                            if (file == FileE) score += KCastlePm;
                            if (file == FileG) score += KCastleBm;
                        }
                    }
                }

                // Back-rank penalties
                if (rank == Rank1 && phase == PhaseMg)
                    score += backrank_p_mg[type];

                score += cmh_dist_alt[sq] * cmhd_mul[phase][type];

                PSQT[phase][p12][sq] = score;
            }
        }
    }
}

template <side_t Side>
int piece_on_semiopen(const Position& pos, int orig)
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

int eval_shelter(const Position& pos, side_t side, int ksq)
{
    u8 oflag  = make_flag(!side);
    u8 mpawn  = make_pawn(side);
    int pincr = side == White ? 8 : -8;
    int kfile = square::file(ksq);

    // Adjust king position if on edge of board

    if (kfile == FileA) ksq++;
    if (kfile == FileH) ksq--;

    int penalty = 0;
    
    for (int offset : { -1, 0, 1}) {
        int orig = ksq + offset;
        int edge = square::dist_edge(orig);

        int p = KingShelterP[edge][7];

        for (int sq = orig + pincr; sq >= square::A2 && sq <= square::H7; sq += pincr) {
            u8 piece = pos.board(sq);

            if (piece & oflag)
                break;
            
            else if (piece == mpawn) {
                int rank = square::rank(sq, side);

                p = KingShelterP[edge][rank];

                break;
            }
        }
        
        penalty += p;
    }

    if (penalty == 0) penalty = KingSafetyD;

    return penalty;
}

int eval_storm(const Position& pos, side_t side, int ksq)
{
    u8 opawn  = make_pawn(!side);
    int pincr = side == White ? 8 : -8;
    int kfile = square::file(ksq);

    // Adjust king position if on edge of board
    
    if (kfile == FileA) ksq++;
    if (kfile == FileH) ksq--;

    int penalty = 0;
   
    for (int offset : { -1, 0, 1 }) {
        int rank = Rank1;
        int edge = square::dist_edge(ksq + offset);
       
        for (int sq = ksq + pincr + offset; sq >= square::A2 && sq <= square::H7; sq += pincr) {
            if (pos.board(sq) == opawn) {
                rank = square::rank(sq, side);
                break;
            }
        }
    
        penalty += KingStormP[edge][rank];
    }
    
    return penalty;
}

int eval_king(const Position& pos, side_t side)
{
    int ksq = pos.king64(side);

    int shelter = eval_shelter(pos, side, ksq) + eval_storm(pos, side, ksq);

    if (pos.can_castle_k(side)) {
        int sq = side == White ? square::G1 : square::G8;
        int sh = eval_shelter(pos, side, sq) + eval_storm(pos, side, sq);

        shelter = max(shelter, sh);
    }
    
    if (pos.can_castle_q(side)) {
        int sq = side == White ? square::C1 : square::C8;
        int sh = eval_shelter(pos, side, sq) + eval_storm(pos, side, sq);

        shelter = max(shelter, sh);
    }

    return shelter;
}

// a la Ethereal
[[maybe_unused]]
int eval_complexity(const Position& pos, int score)
{
    u64 pbb = pos.bb(Pawn);

    int sign    = (score > 0) - (score < 0);
    int pawns   = popcount(pbb);
    int minors  = pos.minors();
    int majors  = pos.majors();
    bool flanks = (pbb & bb::Files[FileA]) && (pbb & bb::Files[FileH]);

    int c = ComplexityPawnsTotal    * pawns
          + ComplexityPawnsFlanked  * flanks
          + ComplexityPawnsEndgame  * !(minors || majors)
          + ComplexityAdjustment;

    int v = sign * max(c, -abs(score));

    return v;
}

// a la Ethereal
[[maybe_unused]]
int eval_scale(const Position& pos, int score)
{
    int wp, wn, wlb, wdb, wr, wq;
    int bp, bn, blb, bdb, br, bq;

    pos.counts(wp, wn, wlb, wdb, wr, wq, White);
    pos.counts(bp, bn, blb, bdb, br, bq, Black);

    // Opposite colored bishops

    if (wlb + wdb == 1 && blb + bdb == 1 && wlb + blb == 1) {
        if (!(wr + wq + br + bq) && wn == 1 && bn == 1)
            return ScaleOcbOneKnight;

        if (!(wn + wq + bn + bq) && wr == 1 && br == 1)
            return ScaleOcbOneRook;

        if (!(wn + wr + wq + bn + br + bq))
            return ScaleOcbBishopsOnly;
    }
    
    int lo_pawns;
    int lo_pieces;
    int hi_pawns;
    int hi_minors;
    int hi_total;

    if (score < 0) {
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
        return ScaleLoneQueen;

    if (hi_minors && hi_total == 1)
        return 0;

    if (wq + bq == 0 && wpieces < 2 && bpieces < 2 && hi_pawns - lo_pawns > 2)
        return ScaleLargePawnAdv;

    return min(128, ScaleM * hi_pawns + ScaleB);
}

template <side_t Side>
int minor_on_outpost(const Position& pos, int orig)
{
    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);

    int rank = square::rank(orig, Side);
    int file = square::file(orig);

    int bonus = 0;

    if (file >= FileB && file <= FileG && rank >= Rank4 && rank <= Rank6)
        bonus = cmh_dist_alt[orig];

    if (bonus == 0) return 0;

    int mp = popcount(mpbb & PawnAttacks[!Side][orig]);

    if (mp == 0) return 0;

    if (opbb & bb::PawnSpanAdj[Side][orig]) return 0;

    return bonus * mp;
}

template <side_t Side>
Value eval_pieces(const Position& pos)
{
    Value val;

    constexpr u8 mpawn  = make_pawn(Side);
    constexpr int pincr = Side == White ? 8 : -8;

    int mksq = pos.king64(Side);
    int oksq = pos.king64(!Side);

    u64 occ = pos.occ();

    u64 mpbb = pos.bb(Side, Pawn);
    u64 mnbb = pos.bb(Side, Knight);
    u64 mbbb = pos.bb(Side, Bishop);
    u64 mrbb = pos.bb(Side, Rook);
    u64 mqbb = pos.bb(Side, Queen);

    u64 opbb = pos.bb(!Side, Pawn);

    u64 rank7 = Side == White ? bb::Rank7 : bb::Rank2;

    int mp = popcount(mpbb);

    u64 bb  = mnbb | mbbb | mrbb | mqbb;

    while (bb) {
        int orig = bb::pop(bb);
        int type = P256ToP6[pos.board(orig)];

        int file = square::file(orig);
        int rank = square::rank(orig, Side);

        if (type == Knight) {
            int outpost = minor_on_outpost<Side>(pos, orig);

            val.mg += KnightOutpostBonusMg * outpost;
            val.eg += KnightOutpostBonusEg * outpost;

            val.mg += KnightPawnBonusMMg * (mp - KnightPawnBonusBAg);
            val.eg += KnightPawnBonusMEg * (mp - KnightPawnBonusBAg);

            if (sq64_is_ok(orig + pincr) && pos.board(orig + pincr) == mpawn) {
                val.mg += MinorBehindPawnBonusMg;
                val.eg += MinorBehindPawnBonusEg;
            }

            int penalty = square::dist(orig, mksq) * square::dist(orig, oksq);

            val.mg += KnightDistOffsetMg - penalty;
            val.eg += KnightDistOffsetEg - penalty;
        }

        else if (type == Bishop) {
            int outpost = minor_on_outpost<Side>(pos, orig);

            val.mg += BishopOutpostBonusMg * outpost;
            val.eg += BishopOutpostBonusEg * outpost;
            
            val.mg += BishopPawnBonusMMg * (mp - BishopPawnBonusBAg);
            val.eg += BishopPawnBonusMEg * (mp - BishopPawnBonusBAg);
            
            if (sq64_is_ok(orig + pincr) && pos.board(orig + pincr) == mpawn) {
                val.mg += MinorBehindPawnBonusMg;
                val.eg += MinorBehindPawnBonusEg;
            }

            u64 att = bb::Leorik::Bishop(orig, occ);

            if (att & mqbb) {
                val.mg += BishopQueenBonusMg;
                val.eg += BishopQueenBonusEg;
            }

            u64 mask = bb::test(bb::Light, orig) ? bb::Light : bb::Dark;
            int count = popcount(mpbb & mask);

            val.mg += BishopPawnSquareBonusMg * count;
            val.eg += BishopPawnSquareBonusEg * count;
        }

        else if (type == Rook) {
            int kfile    = square::file(oksq);
            int krank    = square::rank(oksq, Side);
            int kfdiff   = abs(file - kfile);
            int semiopen = piece_on_semiopen<Side>(pos, orig);
            
            if (rank == Rank7 && (krank == Rank8 || (opbb & rank7))) {
                val.mg += RookSeventhRankBonusMg;
                val.eg += RookSeventhRankBonusEg;
            }

            if (semiopen == FileClosed) {
                val.mg += RookClosedPm;
                val.eg += RookClosedPe;
            }
            else if (kfdiff == 0) {
                val.mg += semiopen == FileSemi ? RookSemiSameKingBonusMg : RookOpenSameKingBonusMg;
                val.eg += semiopen == FileSemi ? RookSemiSameKingBonusEg : RookOpenSameKingBonusEg;
            }
            else if (kfdiff == 1) {
                val.mg += semiopen == FileSemi ? RookSemiAdjKingBonusMg : RookOpenAdjKingBonusMg;
                val.eg += semiopen == FileSemi ? RookSemiAdjKingBonusEg : RookOpenAdjKingBonusEg;
            }
            else {
                val.mg += semiopen == FileSemi ? RookSemiBonusMg : RookOpenBonusMg;
                val.eg += semiopen == FileSemi ? RookSemiBonusEg : RookOpenBonusEg;
            }

            val.mg += RookPawnBonusMMg * (mp - RookPawnBonusBAg);
            val.eg += RookPawnBonusMEg * (mp - RookPawnBonusBAg);
           
            u64 att = bb::Leorik::Rook(orig, occ);

            if (att & mrbb) {
                val.mg += RookRookBonusMg;
                val.eg += RookRookBonusEg;
            }
            
            if (att & mqbb) {
                val.mg += RookQueenBonusMg;
                val.eg += RookQueenBonusEg;
            }
        }

        else if (type == Queen) {
            int krank = square::rank(oksq, Side);

            if (rank == Rank7 && (krank == Rank8 || (opbb & rank7))) {
                val.mg += QueenSeventhRankBonusMg;
                val.eg += QueenSeventhRankBonusEg;
            }
        }
    }
    
    return val;
}

Value eval_mob_ks(const Position& pos)
{
    Value sval[2];

    const u64 kzone[2] = {
        KingZone[pos.king64(White)],
        KingZone[pos.king64(Black)]
    };
    
    u64 patt[2] = {
        patt[White] = bb::PawnAttacks(pos.bb(White, Pawn), White),
        patt[Black] = bb::PawnAttacks(pos.bb(Black, Pawn), Black)
    };
    
    u64 occ = pos.occ();

    for (side_t side : { White, Black }) {
        int natt = 0;
        int batt = 0;
        int ratt = 0;
        int qatt = 0;

        u64 targets = ~(pos.occ(!side) | patt[side]);

        for (u64 bb = pos.bb(!side, Knight); bb; ) {
            int sq = bb::pop(bb);

            u64 att = PieceAttacks[Knight][sq] & targets;
        
            int mob = popcount(att);

            sval[!side].mg += NMobFactorMg * mob;
            sval[!side].eg += NMobFactorEg * mob;

            natt += (att & kzone[side]) != 0;
        }
    
        for (u64 bb = pos.bb(!side, Bishop); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Bishop(sq, occ) & targets;

            int mob = popcount(att);

            sval[!side].mg += BMobFactorMg * mob;
            sval[!side].eg += BMobFactorEg * mob;

            batt += (att & kzone[side]) != 0;
        }
        
        for (u64 bb = pos.bb(!side, Rook); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Rook(sq, occ) & targets;

            int mob = popcount(att);

            sval[!side].mg += RMobFactorMg * mob;
            sval[!side].eg += RMobFactorEg * mob;

            ratt += (att & kzone[side]) != 0;
        }

        for (u64 bb = pos.bb(!side, Queen); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Queen(sq, occ) & targets;

            int mob = popcount(att);

            sval[!side].mg += QMobFactorMg * mob;
            sval[!side].eg += QMobFactorEg * mob;

            qatt += (att & kzone[side]) != 0;
        }

        if (pos.phase_inv(!side) < 4) continue;

        double att_count = qatt + ratt + batt + natt;
        double att_value = 4 * qatt + 2 * ratt + batt + natt;
   
        int e = !att_count ? 0 : KingSafetyA / (1.0 + exp(-(KingSafetyB / 10.0) * (att_count - (KingSafetyC / 10.0)))) * att_value;
   
        sval[side].mg += -e;
        sval[side].mg += eval_king(pos, side);
    }

    return sval[White] - sval[Black];
}

Value eval_pattern(const Position& pos)
{
    Value val;

    // White
    
    int wksq = pos.king64(White);
    
    if (pos.board(square::C1) == WB256 && pos.board(square::D2) == WP256 && pos.board(square::D3) != PieceNone256)
        val.mg += BishopBlockedPm;
    if (pos.board(square::F1) == WB256 && pos.board(square::E2) == WP256 && pos.board(square::E3) != PieceNone256)
        val.mg += BishopBlockedPm;

    if ((pos.board(square::A7) == WB256 && pos.board(square::B6) == BP256) || (pos.board(square::B8) == WB256 && pos.board(square::C7) == BP256))
        val.mg += BishopTrappedPm;
    if ((pos.board(square::H7) == WB256 && pos.board(square::G6) == BP256) || (pos.board(square::G8) == WB256 && pos.board(square::F7) == BP256))
        val.mg += BishopTrappedPm;
    if (pos.board(square::A6) == WB256 && pos.board(square::B5) == BP256)
        val.mg += BishopTrappedPm / 2;
    if (pos.board(square::H6) == WB256 && pos.board(square::G5) == BP256)
        val.mg += BishopTrappedPm / 2;
    
    if ((pos.board(square::A1) == WR256 || pos.board(square::A2) == WR256 || pos.board(square::B1) == WR256) && (wksq == square::B1 || wksq == square::C1))
        val.mg += RookBlockedPm;
    if ((pos.board(square::H1) == WR256 || pos.board(square::H2) == WR256 || pos.board(square::G1) == WR256) && (wksq == square::F1 || wksq == square::G1))
        val.mg += RookBlockedPm;

    // Black
    
    int bksq = pos.king64(Black);

    if (pos.board(square::C8) == BB256 && pos.board(square::D7) == BP256 && pos.board(square::D6) != PieceNone256)
        val.mg -= BishopBlockedPm;
    if (pos.board(square::F8) == BB256 && pos.board(square::E7) == BP256 && pos.board(square::E6) != PieceNone256)
        val.mg -= BishopBlockedPm;

    if ((pos.board(square::A2) == BB256 && pos.board(square::B3) == WP256) || (pos.board(square::B1) == BB256 && pos.board(square::C2) == WP256))
        val.mg -= BishopTrappedPm;
    if ((pos.board(square::H2) == BB256 && pos.board(square::G3) == WP256) || (pos.board(square::G1) == BB256 && pos.board(square::F2) == WP256))
        val.mg -= BishopTrappedPm;
    if (pos.board(square::A3) == BB256 && pos.board(square::B4) == WP256)
        val.mg -= BishopTrappedPm / 2;
    if (pos.board(square::H3) == BB256 && pos.board(square::G4) == WP256)
        val.mg -= BishopTrappedPm / 2;
    
    if ((pos.board(square::A8) == BR256 || pos.board(square::A7) == BR256 || pos.board(square::B8) == BR256) && (bksq == square::B8 || bksq == square::C8))
        val.mg -= RookBlockedPm;
    if ((pos.board(square::H8) == BR256 || pos.board(square::H7) == BR256 || pos.board(square::G8) == BR256) && (bksq == square::F8 || bksq == square::G8))
        val.mg -= RookBlockedPm;

    return val;
}

int eval_internal(const Position& pos)
{
    assert(!pos.checkers());

    EvalEntry eentry;

    if (etable.get(pos.key(), eentry))
        return eentry.score + TempoB;
    
    int ValuePiece[6][2] = {
        { ValuePawnMg,   ValuePawnEg },
        { ValueKnightMg, ValueKnightEg },
        { ValueBishopMg, ValueBishopEg },
        { ValueRookMg,   ValueRookEg },
        { ValueQueenMg,  ValueQueenEg },
        { 0, 0 }
    };

    Value sval[2];

    for (side_t side : { White, Black }) {
        for (u64 bb = pos.occ(side); bb; bb &= (bb - 1)) {
            int sq = countr_zero(bb);
            int p12 = P256ToP12[pos.board(sq)];

            // Material
            
            sval[side].mg += ValuePiece[p12 / 2][PhaseMg];
            sval[side].eg += ValuePiece[p12 / 2][PhaseEg];

            // PSQT
            
            sval[side].mg += PSQT[PhaseMg][p12][sq];
            sval[side].eg += PSQT[PhaseEg][p12][sq];
        }

        if (pos.bishop_pair(side)) {
            sval[side].mg += BishopPairBMg;
            sval[side].eg += BishopPairBEg;
        }
    }
    
    Value val = sval[White] - sval[Black];

    // Mobility and King Safety

    val += eval_mob_ks(pos);

    // Patterns

    val += eval_pattern(pos);

    // Pawn prefetch (misplaced?)
    
    if (pos.pawns())
        ptable.prefetch(pos.pawn_key());

    // Major and minor pieces
   
    val += eval_pieces<White>(pos);
    val -= eval_pieces<Black>(pos);

    // Evaluation of pawns is done separately because all pawns share a hash entry

    if (pos.pawns()) {
        PawnEntry pentry;

        val += eval_pawns(pos, pentry);
        val += eval_passers(pos, pentry);
    }

    val.eg += eval_complexity(pos, val.eg);

    int factor = eval_scale(pos, val.eg);
    int score = val.lerp(pos.phase(), factor);

    if (pos.side() == Black) score = -score;

    eentry.score = score;

    etable.set(pos.key(), eentry);
    
    return score + TempoB;
}

int eval(const Position& pos, int ply)
{
    if (ply > 0 && mstack.back() == MoveNull)
        return -Evals[ply - 1] + 2 * TempoB;

#if PROFILE >= PROFILE_SOME
    gstats.evals_count++;
#if PROFILE >= PROFILE_ALL
    Timer timer(true);
#endif
#endif

    int e = eval_internal(pos);
   
#if PROFILE >= PROFILE_ALL
    timer.stop();
    gstats.time_eval_ns += timer.elapsed_time<Timer::Nano>();
    gstats.cycles_eval += timer.elapsed_cycles();
#endif

    return e;
}

// Begin Tuner

#ifdef TUNE

struct Param {
    string name;
    int * pvalue;
    int init;
    int min;
    int max;
    int start;
    
    Param(string n, int min_, int max_, int * p)
        : name(n), 
          pvalue(p),
          min(min_),
          max(max_),
          start(*p)
    {
#if 1
        uniform_int_distribution<int> gen(min_, max_);
        init = gen(mt);
#else
        init = *p;
#endif
    }
    
    Param(string n, int * p, int delta) : Param(n, *p - delta, *p + delta, p) { }
    Param(string n, int * p, int min_, int max_) : Param(n, min_, max_, p) { }
};

vector<Position> g_pos;
vector<Param> g_params;

double K_VALUE = 2.41623;

double sigmoid(const Position& pos, int eval, double K)
{
    if (pos.side() == Black) eval = -eval;

    double e = -K * eval / 400;

    return 1 / (1 + exp(e));
}

double eval_error(const Position& pos, double K)
{
    int    ev     = eval(pos);
    double error  = pow(pos.outcome() - sigmoid(pos, ev, K), 2);

    return error;
}

double eval_error(const vector<Position>& v, double K)
{
    double sse = 0;

    #pragma omp parallel num_threads(11)
    {
        double se   = 0;
        size_t id   = omp_get_thread_num();
        size_t num  = omp_get_num_threads();

        for (size_t i = id; i < v.size(); i += num)
            se += eval_error(v[i], K);

        #pragma omp critical
        {
            sse += se;
        }
    }

    return sse / v.size();
}

void eval_read(vector<Position>& v, string filename, double share)
{
    cout << "Reading tuning file... " << flush;

    string line;

    ifstream ifs(filename);

    uniform_real_distribution<> dist(0.0, 1.0);

    while (getline(ifs, line)) {
        if (dist(mt) > share) continue;
        
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

    cout << "OK" << endl
         << "Parsed " << v.size() << " positions" << endl;
}

double myvfunc(const vector<double>& x, [[maybe_unused]] vector<double>& grad, [[maybe_unused]] void* my_func_data)
{
    static int iter     = 1;
    static int btime    = time(nullptr);
    static double minf  = 1;
    
    for (size_t i = 0; i < x.size(); i++) {
        int xp = clamp(int(x[i]), g_params[i].min, g_params[i].max);

        *g_params[i].pvalue = xp;
    }
   
    // Recompute PSQT
    //eval_init(); 
    
    double f = eval_error(g_pos, K_VALUE);

    stringstream ss;

    ss.precision(7);

    bool better = false;

    if (f < minf) {
        better = true;

        minf = f;

        double sum_se = 0;

        for (auto &param : g_params) {
            int p1 = *param.pvalue;
            int p2 = param.start;
            int d = p1 - p2;
            string info = p1 == param.min ? " LOWER BOUND" :
                          p1 == param.max ? " UPPER BOUND" : "";

            sum_se += pow(d, 2);
        
            ss << "int " << setw(28) << setfill(' ') << left << param.name << " = " << right
               << setw(8) << setfill(' ') << *param.pvalue << "; diff = " << d 
               << info << endl;
        }

        double rms = sqrt(sum_se / g_params.size());

        ss << "minf=" << minf 
           << ",iter=" << iter 
           << ",time=" << (time(nullptr) - btime) 
           << ",rms=" << int(rms) << endl;
    }

    if (!ss.str().empty())
        cout << ss.str();

    if (better) {
        fstream ofile;

        ofile.open("opt.txt", ios::out | ios::app);
        ofile << ss.str();
        ofile.flush();
        ofile.close();
    }

    iter++;

    return f;
}

vector<double> tune_init(nlopt::opt& opt)
{
    vector<double> lb;
    vector<double> x;
    vector<double> ub;

    for (auto &param : g_params) {
        lb.push_back(param.min);
        x.push_back(param.init);
        ub.push_back(param.max);
    }

    opt.set_lower_bounds(lb);
    opt.set_upper_bounds(ub);

    opt.set_min_objective(myvfunc, nullptr);

    return x;
}

// Golden-section search
double eval_K(double a, double b, double tol, double& best)
{
    double invphi  = (sqrt(5) - 1) / 2;
    double invphi2 = (3 - sqrt(5)) / 2;

    double h = b - a;

    double c = a + invphi2 * h;
    double yc = eval_error(g_pos, c);
    double d = a + invphi  * h;
    double yd = eval_error(g_pos, d);
    
    size_t n = std::ceil(log(tol / h) / log(invphi));

    for (size_t i = 0; i < n; i++) {
        if (yc < yd) { // yc > yd to find the maximum
            b = d;
            d = c;
            yd = yc;
            h = invphi * h;
            c = a + invphi2 * h;
            yc = eval_error(g_pos, c);
        }
        else {
            a = c;
            c = d;
            yc = yd;
            h = invphi * h;
            d = a + invphi * h;
            yd = eval_error(g_pos, d);
        }
    }

    best = (yc + yd) / 2;

    return (yc < yd ? a + d : b + c) / 2;
}

void eval_tune(int argc, char* argv[])
{
    Tuner tuner;
    
    Tokenizer fields(argc, argv);

    for (size_t i = 0; i < fields.size(); i++) {
        Tokenizer args(fields[i], '=');
        
        string k = args.get(0);
        string v = args.get(1, "");

        if (k == "bench")
            tuner.bench = true;
        else if (k == "debug")
            tuner.debug = true;
        else if (k == "file")
            tuner.path = v;
        else if (k == "ktune")
            tuner.ktune = true;
        else if (k == "opt")
            tuner.opt = true;
        else if (k == "ratio")
            tuner.ratio = stoull(v) / 100.0;
        else if (k == "seed")
            tuner.seed = stoull(v);
        else if (k == "time")
            tuner.time = stod(v);
    }

    mt = tuner.seed ? mt19937_64(tuner.seed) : mt19937_64(rd());
        
    eval_read(g_pos, tuner.path, tuner.ratio);

    if (tuner.bench) {
        i64 btime = Timer::now();
        cout << eval_error(g_pos, K_VALUE) << endl;
        i64 etime = Timer::now();

        cout << etime - btime << " ms" << endl;
        cout << g_pos.size() / (etime - btime) << " ke/s" << endl;
    }
   
    if (tuner.debug) {
        for (auto &pos : g_pos) {
            double error = eval_error(pos, K_VALUE);

            if (error >= 0.99) {
                cerr << pos.outcome() << ','
                     << eval(pos) << ','
                     << pos.to_fen() << endl;
            }
        }
    }
    
    if (tuner.ktune) {
        double best;
        double K = eval_K(0, 6, 1e-8, best);

        cout << "Optimal K value = " << K << ", error = " << best << endl;

        K_VALUE = K;
    }

    if (!tuner.opt)
        return;

#define MP(name1) MPS(name1), &name1
#define MPS(name2) #name2
#define GPPB g_params.push_back

#if 1
    // Piece Values : 13
    //GPPB(Param(MP(ValuePawnMg),       50,  100));
    //GPPB(Param(MP(ValuePawnEg),      100,  200));

    GPPB(Param(MP(ValueKnightMg),    100,  900));
    GPPB(Param(MP(ValueKnightEg),    100,  900));
    GPPB(Param(MP(ValueBishopMg),    100,  900));
    GPPB(Param(MP(ValueBishopEg),    100,  900));
    GPPB(Param(MP(ValueRookMg),      150, 1500));
    GPPB(Param(MP(ValueRookEg),      150, 1500));
    GPPB(Param(MP(ValueQueenMg),     300, 3000));
    GPPB(Param(MP(ValueQueenEg),     300, 3000));
   
    GPPB(Param(MP(BishopPairBMg),      0,  100));
    GPPB(Param(MP(BishopPairBEg),      0,  150));

    GPPB(Param(MP(TempoB),             1,   50));
#endif

#if 0
    // PSQT : 55
    GPPB(Param(MP(NPSFactorEg),   0,  50));
    GPPB(Param(MP(BPSFactorEg),   0,  50));
    GPPB(Param(MP(RPSFactorEg),   0,  50));
    GPPB(Param(MP(QPSFactorEg),   0, 100));
    GPPB(Param(MP(KPSFactorMg), -50,   0));
    GPPB(Param(MP(KPSFactorEg),   0,  50));

    GPPB(Param(MP(NRank1Pm), -50, 0));
    GPPB(Param(MP(BRank1Pm), -50, 0));

    GPPB(Param(MP(PRank5Eg), 0, 250));
    GPPB(Param(MP(PRank6Eg), 0, 250));
    GPPB(Param(MP(PRank7Eg), 0, 250));

    GPPB(Param(MP(PFileAHBm), -100, 100));
    GPPB(Param(MP(PFileAHBe), -100, 100));
    GPPB(Param(MP(PFileBGBm), -100, 100));
    GPPB(Param(MP(PFileBGBe), -100, 100));
    GPPB(Param(MP(PFileCFBm), -100, 100));
    GPPB(Param(MP(PFileCFBe), -100, 100));
    GPPB(Param(MP(PFileDEBm), -100, 100));
    GPPB(Param(MP(PFileDEBe), -100, 100));

    GPPB(Param(MP(NRank4Bm), 0, 100));
    GPPB(Param(MP(NRank4Be), 0, 100));
    GPPB(Param(MP(NRank5Bm), 0, 100));
    GPPB(Param(MP(NRank5Be), 0, 100));
    GPPB(Param(MP(NRank6Bm), 0, 100));
    GPPB(Param(MP(NRank6Be), 0, 100));

    GPPB(Param(MP(RRank7Bm), 0, 100));
    GPPB(Param(MP(RRank7Be), 0, 100));
    GPPB(Param(MP(RRank8Bm), 0, 100));
    GPPB(Param(MP(RRank8Be), 0, 100));

    GPPB(Param(MP(QRank7Be), 0, 250));
    GPPB(Param(MP(QRank8Be), 0, 250));

    GPPB(Param(MP(NCornerPm), -100, 0));
    GPPB(Param(MP(NCornerPe), -100, 0));
    GPPB(Param(MP(KCornerPm), -100, 0));
    GPPB(Param(MP(KCornerPe), -100, 0));
    
    GPPB(Param(MP(RFileAHBm), -50, 50));
    GPPB(Param(MP(RFileAHBe), -50, 50));
    GPPB(Param(MP(RFileBGBm), -50, 50));
    GPPB(Param(MP(RFileBGBe), -50, 50));
    GPPB(Param(MP(RFileCFBm), -50, 50));
    GPPB(Param(MP(RFileCFBe), -50, 50));
    GPPB(Param(MP(RFileDEBm), -50, 50));
    GPPB(Param(MP(RFileDEBe), -50, 50));

    GPPB(Param(MP(QFileAHBe), -100, 100));
    GPPB(Param(MP(QFileBGBe), -100, 100));
    GPPB(Param(MP(QFileCFBe), -100, 100));
    GPPB(Param(MP(QFileDEBe), -100, 100));
    
    GPPB(Param(MP(KCastleBm),   0, 50));
    GPPB(Param(MP(KCastlePm), -50,  0));
#endif

#if 0
    // Mobility : 12
    GPPB(Param(MP(NMobFactorMg),   0, 25));
    GPPB(Param(MP(NMobFactorEg),   0, 25));
    GPPB(Param(MP(BMobFactorMg),   0, 25));
    GPPB(Param(MP(BMobFactorEg),   0, 25));
    GPPB(Param(MP(RMobFactorMg),   0, 25));
    GPPB(Param(MP(RMobFactorEg),   0, 25));
    GPPB(Param(MP(QMobFactorMg),   0, 25));
    GPPB(Param(MP(QMobFactorEg),   0, 25));
#endif

#if 0
    // King Safety : 4
    GPPB(Param(MP(KingSafetyA),   10, 50));
    GPPB(Param(MP(KingSafetyB),   10, 30));
    GPPB(Param(MP(KingSafetyC),   10, 30));
    GPPB(Param(MP(KingSafetyD),  -50, -1));

    GPPB(Param(MP(KingStormP[0][2]), -100, 100));
    GPPB(Param(MP(KingStormP[0][3]), -100, 100));
    GPPB(Param(MP(KingStormP[0][4]), -100, 100));
    GPPB(Param(MP(KingStormP[1][2]), -100, 100));
    GPPB(Param(MP(KingStormP[1][3]), -100, 100));
    GPPB(Param(MP(KingStormP[1][4]), -100, 100));
    GPPB(Param(MP(KingStormP[2][2]), -100, 100));
    GPPB(Param(MP(KingStormP[2][3]), -100, 100));
    GPPB(Param(MP(KingStormP[2][4]), -100, 100));
    GPPB(Param(MP(KingStormP[3][2]), -100, 100));
    GPPB(Param(MP(KingStormP[3][3]), -100, 100));
    GPPB(Param(MP(KingStormP[3][4]), -100, 100));
   
    GPPB(Param(MP(KingShelterP[0][2]), -100, 100));
    GPPB(Param(MP(KingShelterP[0][3]), -100, 100));
    GPPB(Param(MP(KingShelterP[0][4]), -100, 100));
    GPPB(Param(MP(KingShelterP[0][5]), -100, 100));
    GPPB(Param(MP(KingShelterP[0][6]), -100, 100));
    GPPB(Param(MP(KingShelterP[0][7]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][2]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][3]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][4]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][5]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][6]), -100, 100));
    GPPB(Param(MP(KingShelterP[1][7]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][2]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][3]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][4]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][5]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][6]), -100, 100));
    GPPB(Param(MP(KingShelterP[2][7]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][2]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][3]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][4]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][5]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][6]), -100, 100));
    GPPB(Param(MP(KingShelterP[3][7]), -100, 100));
#endif
    
#if 0
    // Pieces : 47
    GPPB(Param(MP(MinorBehindPawnBonusMg),     0,  50));
    GPPB(Param(MP(MinorBehindPawnBonusEg),     0,  50));

    GPPB(Param(MP(KnightOutpostBonusMg),       0,  50));
    GPPB(Param(MP(KnightOutpostBonusEg),       0,  50));
    GPPB(Param(MP(KnightPawnBonusMMg),       -25,  25));
    GPPB(Param(MP(KnightPawnBonusMEg),       -25,  25));
    GPPB(Param(MP(KnightPawnBonusBAg),         0,   8));
    GPPB(Param(MP(KnightDistOffsetMg),         0,  50));
    GPPB(Param(MP(KnightDistOffsetEg),         0,  50));

    GPPB(Param(MP(BishopOutpostBonusMg),       0,  50));
    GPPB(Param(MP(BishopOutpostBonusEg),       0,  50));
    GPPB(Param(MP(BishopBlockedPm),         -100,   0));
    GPPB(Param(MP(BishopTrappedPm),         -100,   0));
    GPPB(Param(MP(BishopQueenBonusMg),         0, 100));
    GPPB(Param(MP(BishopQueenBonusEg),         0, 100));
    GPPB(Param(MP(BishopPawnSquareBonusMg),  -10,  10));
    GPPB(Param(MP(BishopPawnSquareBonusEg),  -10,  10));
    GPPB(Param(MP(BishopPawnBonusMMg),       -25,  25));
    GPPB(Param(MP(BishopPawnBonusMEg),       -25,  25));
    GPPB(Param(MP(BishopPawnBonusBAg),         0,   8));

    GPPB(Param(MP(RookBlockedPm),           -100,   0));
    GPPB(Param(MP(RookClosedPm),            -100, 100));
    GPPB(Param(MP(RookClosedPe),            -100, 100));
    GPPB(Param(MP(RookOpenAdjKingBonusMg),     0, 100));
    GPPB(Param(MP(RookOpenAdjKingBonusEg),     0, 100));
    GPPB(Param(MP(RookOpenBonusMg),         -100, 100));
    GPPB(Param(MP(RookOpenBonusEg),         -100, 100));
    GPPB(Param(MP(RookOpenSameKingBonusMg), -200, 200));
    GPPB(Param(MP(RookOpenSameKingBonusEg), -200, 200));
    GPPB(Param(MP(RookRookBonusMg),            0,  50));
    GPPB(Param(MP(RookRookBonusEg),            0,  50));
    GPPB(Param(MP(RookQueenBonusMg),           0, 100));
    GPPB(Param(MP(RookQueenBonusEg),           0, 100));
    GPPB(Param(MP(RookPawnBonusMMg),         -25,  25));
    GPPB(Param(MP(RookPawnBonusMEg),         -25,  25));
    GPPB(Param(MP(RookPawnBonusBAg),           0,   8));
    GPPB(Param(MP(RookSemiAdjKingBonusMg),  -100, 100));
    GPPB(Param(MP(RookSemiAdjKingBonusEg),  -100, 100));
    GPPB(Param(MP(RookSemiBonusMg),         -100, 100));
    GPPB(Param(MP(RookSemiBonusEg),         -100, 100));
    GPPB(Param(MP(RookSemiSameKingBonusMg), -100, 100));
    GPPB(Param(MP(RookSemiSameKingBonusEg), -100, 100));
    GPPB(Param(MP(RookSeventhRankBonusMg),  -100, 100));
    GPPB(Param(MP(RookSeventhRankBonusEg),  -100, 100));

    GPPB(Param(MP(QueenSeventhRankBonusMg), -100, 100));
    GPPB(Param(MP(QueenSeventhRankBonusEg), -100, 100));
#endif

#if 0
    // Pawns : 23
    GPPB(Param(MP(PawnBackOpenPm),        -100, 100));
    GPPB(Param(MP(PawnBackOpenPe),        -100, 100));
    GPPB(Param(MP(PawnBackwardPm),        -100, 100));
    GPPB(Param(MP(PawnBackwardPe),        -100, 100));
    GPPB(Param(MP(PawnCandidateBaseMg),   -100, 100));
    GPPB(Param(MP(PawnCandidateBaseEg),   -100, 100));
    GPPB(Param(MP(PawnCandidateFactorMg), -500, 500));
    GPPB(Param(MP(PawnCandidateFactorEg), -500, 500));
    GPPB(Param(MP(PawnDoubledPm),         -100, 100));
    GPPB(Param(MP(PawnDoubledPe),         -100, 100));
    GPPB(Param(MP(PawnIsoOpenPm),         -100, 100));
    GPPB(Param(MP(PawnIsoOpenPe),         -100, 100));
    GPPB(Param(MP(PawnIsolatedPm),        -100, 100));
    GPPB(Param(MP(PawnIsolatedPe),        -100, 100));

    GPPB(Param(MP(PawnPassedBaseMg),      -100, 100));
    GPPB(Param(MP(PawnPassedBaseEg),      -100, 100));
    GPPB(Param(MP(PawnPassedFactorMg),    -500, 500));
    GPPB(Param(MP(PawnPassedFactorEg),    -500, 500));

    GPPB(Param(MP(PawnPassedFactorA),        0, 100));
    GPPB(Param(MP(PawnPassedFactorB),        1,  50));
    GPPB(Param(MP(PawnPassedFactorC),     -500, 500));
    
    GPPB(Param(MP(PawnAttackMg),             1, 100));
    GPPB(Param(MP(PawnAttackEg),             1, 100));
#endif

#if 0
    // Scaling and Complexity : 11
    GPPB(Param(MP(ComplexityPawnsTotal),      0,  10));
    GPPB(Param(MP(ComplexityPawnsFlanked),    0,  50));
    GPPB(Param(MP(ComplexityPawnsEndgame),    0, 100));
    GPPB(Param(MP(ComplexityAdjustment),   -100, 100));

    GPPB(Param(MP(ScaleOcbBishopsOnly),       0, 250));
    GPPB(Param(MP(ScaleOcbOneKnight),         0, 250));
    GPPB(Param(MP(ScaleOcbOneRook),           0, 250));
    GPPB(Param(MP(ScaleLoneQueen),            0, 250));
    GPPB(Param(MP(ScaleLargePawnAdv),         0, 250));
    GPPB(Param(MP(ScaleM),                    0, 100));
    GPPB(Param(MP(ScaleB),                    0, 100));
#endif

    // Global
    nlopt::opt opt(nlopt::GN_CRS2_LM, g_params.size());
    //nlopt::opt opt(nlopt::GN_ISRES, g_params.size());
    //nlopt::opt opt(nlopt::GN_ESCH, g_params.size());

    // Local
    //nlopt::opt opt(nlopt::LN_SBPLX, g_params.size());
    
    opt.set_xtol_abs(1.0);

    if (tuner.time)
        opt.set_maxtime(60 * 60 * tuner.time);
    
    vector<double> x = tune_init(opt);

    try {
        double minf;

        int btime = time(0);
        opt.optimize(x, minf);
        int etime = time(0);

        cout << "opt complete" << endl;
        cout << (etime - btime) << ',' << minf << endl;

        for (size_t j = 0; j < g_params.size(); j++)
            cout << g_params[j].name << " = " << int(round(x[j])) << endl;

    } catch (exception& e) {
        cout << "nlopt failed: " << e.what() << endl;
    }
}

#endif

// End Tuner
