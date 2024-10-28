#include <bit>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include "attacks.h"
#include "bb.h"
#include "eval.h"
#include "gen.h"
#include "ht.h"
#include "pawn.h"
#include "search.h"

using namespace std;


enum { FileClosed, FileSemiOpen, FileOpen };

HashTable<64 * 1024 * 1024, EvalEntry> etable;

Value PSQTv[12][64];

constexpr int MatValue[2][6] = {
    {  60, 246, 259, 288,  570, 0 },
    { 125, 350, 370, 633, 1203, 0 }
};

constexpr int LOG[32] = {
    0, 1, 2, 3, 3, 4, 4, 4,
    5, 5, 5, 5, 5, 6, 6, 6,
    6, 6, 6, 6, 6, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7
};

int BPairBm                         =       2;
int BPairBe                         =      73;

int RBlockedPm                      =     -42;
int RBlockedPe                      =     -33;

int NQAttBm                         =      50;
int BQAttBm                         =      53;
int BQAttBe                         =      89;
int RQAttBm                         =      73;
int NQDefBm                         =      -2;
int NQDefBe                         =      20;
int BQDefBm                         =       5;
int BQDefBe                         =      12;
int RRSameFileBm                    =      14;
int RRSameFileBe                    =      -2;
int RRSameRankBm                    =       3;
int RRSameRankBe                    =       1;
int RQSameFileBm                    =       3;
int RQSameFileBe                    =      28;
int RQSameRankBm                    =       5;
int RQSameRankBe                    =       7;

int KingSafetyA                     =       5;
int KingSafetyB                     =      13;
int KingSafetyC                     =      21;
int KingSafetyD                     =      25;

int NPinByBAbsPm                    =     -68;
int NPinByBAbsPe                    =    -116;
int NPinByRAbsPm                    =     -39;
int NPinByRAbsPe                    =     -29;
int NPinByQAbsPm                    =     -42;
int NPinByQAbsPe                    =       6;
int BPinByRAbsPm                    =     -32;
int BPinByRAbsPe                    =     -44;
int BPinByQAbsPm                    =     -32;
int BPinByQAbsPe                    =       5;
int RPinByBAbsPm                    =    -100;
int RPinByBAbsPe                    =    -400;
int RPinByQAbsPm                    =     -26;
int RPinByQAbsPe                    =     -68;
int QPinByBRelPm                    =    -125;
int QPinByBRelPe                    =    -280;
int QPinByRRelPm                    =     -97;
int QPinByRRelPe                    =    -289;

int MinorBehindPawnBm               =       8;
int MinorBehindPawnBe               =       6;
int KnightOutpostBm                 =      20;
int KnightOutpostBe                 =      15;
int KnightDistDivPm                 =       2;
int KnightDistDivPe                 =       2;
int BishopOutpostBm                 =      21;
int BishopOutpostBe                 =      12;
int BishopPawnsPm                   =      -1;
int BishopPawnsPe                   =      -3;
int RookClosedPm                    =      15;
int RookClosedPe                    =      35;
int RookOpenAdjKingBm               =      61;
int RookOpenAdjKingBe               =      36;
int RookOpenBm                      =      38;
int RookOpenBe                      =      42;
int RookOpenSameKingBm              =     157;
int RookOpenSameKingBe              =      33;
int RookSemiAdjKingBm               =      32;
int RookSemiAdjKingBe               =      29;
int RookSemiBm                      =      19;
int RookSemiBe                      =      61;
int RookSemiSameKingBm              =      69;
int RookSemiSameKingBe              =       3;
int RookSeventhRankBm               =     -20;
int RookSeventhRankBe               =      19;
int QueenSeventhRankBm              =     -47;
int QueenSeventhRankBe              =      19;

int PawnBackOpenPm                  =     -18;
int PawnBackOpenPe                  =     -10;
int PawnBackwardPm                  =      -1;
int PawnBackwardPe                  =     -15;
int PawnDoubledPm                   =      -9;
int PawnDoubledPe                   =      -6;
int PawnIsoOpenPm                   =     -14;
int PawnIsoOpenPe                   =     -17;
int PawnIsolatedPm                  =       0;
int PawnIsolatedPe                  =     -20;
int PawnConnectedPB                 =       2;
int PawnPassedGaurdedBm             =      15;
int PawnPassedGaurdedBe             =      16;
int PawnPassedStopOffset            =      12;

int ComplexityPawnsTotal            =       5;
int ComplexityPawnsFlanked          =      55;
int ComplexityPawnsEndgame          =     141;
int ComplexityAdjustment            =     -99;

int ScaleOcbBishopsOnly             =      58;
int ScaleOcbOneKnight               =     102;
int ScaleOcbOneRook                 =     115;
int ScaleLoneQueen                  =     109;
int ScaleLargePawnAdv               =     146;
int ScalePNone                      =      47;
int ScalePOne                       =     119;
int ScalePTwo                       =     137;
int TempoB                          =      24;

int MobB[2][4][8] = {
    {
        {-25,-8,-1,5,12,21,0,0},
        {-12,-3,4,11,18,21,47,0},
        {-9,-4,0,3,7,11,22,0},
        {6,8,9,13,20,27,35,84}
    },
    {
        {40,78,97,117,132,126,0,0},
        {33,57,68,89,108,114,103,0},
        {84,104,104,115,133,153,153,0},
        {276,234,259,282,298,317,341,318}
    }
};

int PSQTi[2][6][32] = {
    {
        {0,0,0,0,59,62,55,55,58,55,55,57,60,59,57,61,67,65,55,73,73,79,86,99,44,42,76,96,0,0,0,0},
        {192,223,224,235,227,222,238,241,229,242,243,252,246,261,257,254,268,247,267,260,245,258,276,271,243,223,274,267,136,179,142,219},
        {261,269,248,246,260,263,266,253,255,262,262,258,258,257,260,272,242,261,267,271,257,253,266,261,226,241,239,238,211,209,158,162},
        {277,283,288,293,274,277,282,283,272,282,272,277,271,279,271,277,280,285,287,295,291,307,303,316,311,304,319,311,305,308,274,282},
        {561,570,565,570,570,577,577,576,573,574,571,566,568,572,561,561,561,563,555,550,564,564,563,540,594,583,591,556,578,583,590,572},
        {3,0,-14,-44,21,22,2,0,14,14,19,-1,-16,9,4,-21,-8,7,-8,-28,-2,33,3,-55,-10,104,32,83,-4,110,87,49}
    },
    {
        {0,0,0,0,127,132,145,138,125,124,128,128,127,127,123,118,137,132,122,111,153,147,129,103,159,144,117,92,0,0,0,0},
        {330,320,334,339,328,345,338,346,334,349,351,369,352,357,373,379,347,360,373,380,339,348,370,369,334,356,348,361,322,338,365,352},
        {366,363,364,368,358,365,366,374,369,378,384,391,369,377,390,393,375,381,381,395,372,388,388,382,377,386,390,388,384,391,398,400},
        {625,627,629,624,612,622,624,624,629,633,640,636,647,655,661,654,657,666,666,662,661,659,665,658,654,658,659,659,667,670,682,674},
        {1182,1166,1172,1183,1178,1175,1182,1186,1195,1208,1220,1220,1218,1230,1248,1251,1230,1260,1269,1284,1224,1250,1279,1297,1234,1254,1264,1294,1239,1247,1240,1250},
        {-39,-19,-14,-18,-15,-7,0,-3,-16,-5,1,8,-11,9,18,24,1,38,40,37,20,63,62,60,8,36,53,17,-107,-34,-18,-9}
    }
};

int PawnCandidateB[8] = {0,-6,10,16,27,138,0,0};

int PawnConnectedB[8] = {0,0,1,5,14,28,34,0};

int PawnPassedB[2][3][8] = {
    {
        {5,0,0,0,26,58,144,0},
        {0,0,0,0,0,0,0,0},
        {7,11,10,9,25,65,132,0}
    },
    {
        {16,0,12,51,81,121,208,21},
        {3,56,52,186,342,548,711,12},
        {15,4,18,65,118,221,376,19}
    }
};

int PawnAttackB[2][2][6] = {
    {
        {0,44,65,64,52,0},
        {0,50,59,51,42,0}
    },
    {
        {0,57,69,32,78,0},
        {0,58,91,105,97,0}
    }
};

int PawnAttackPushB[2][2][6] = {
    {
        {0,6,3,8,6,0},
        {0,7,10,5,11,0}
    },
    {
        {0,10,1,13,1,0},
        {0,24,8,31,13,0}
    }
};

int KingShelterP[4][8] = {
    {0,0,0,-11,-32,-17,-21,-15},
    {0,0,-11,-31,-40,-18,-48,-36},
    {0,0,-20,-24,-29,-11,-5,-34},
    {0,0,-24,-25,-32,-23,-5,-31}
};

int KingStormP[4][8] = {
    {0,0,-26,-14,-5,0,0,0},
    {0,0,-29,-8,-3,0,0,0},
    {0,0,-53,-27,-1,0,0,0},
    {0,0,-38,-30,-7,0,0,0}
};

int KingSafetyW[4][8] = {
    {0,221,252,108,115,0,0,0},
    {0,-110,357,127,65,72,70,73},
    {0,329,329,248,109,67,9,9},
    {0,0,910,391,169,48,-5,-50}
};

int PiecePawnOffset[2][6] = {
    {0,1,-1,-3,0,0},
    {0,0,-9,-7,0,0}
};

int RookKnightImbB[2] = {11,32};
int RookBishopImbB[2] = {7,14};
int RookTwoMinorsImbB[2] = {-42,-21};
int QueenTwoRooksImbB[2] = {91,-1};
int QueenRookKnightImbB[2] = {50,-16};
int QueenRookBishopImbB[2] = {39,-10};

template <side_t Side> static int piece_on_file     (const Position& pos, int orig);
template <side_t Side> static bool minor_on_outpost (const Position& pos, int orig);
template <side_t Side> static Value eval_pieces     (const Position& pos);
template <side_t Side> static Value eval_imbalance  (const Position& pos);

static int eval_shelter (const Position& pos, side_t side, int ksq);
static int eval_storm   (const Position& pos, side_t side, int ksq);
static int eval_scale   (const Position& pos, int score);

static Value eval_complexity(const Position& pos, int score);

static Value reset_psqt(int piece, int sq)
{
    assert(piece12_is_ok(piece));
    assert(sq64_is_ok(sq));

    int file = sq & 7;
    int rank = sq >> 3;

    file ^= 7 * (file >= 4);
    rank ^= 7 * (piece & 1);

    sq = 4 * rank + file;

    Value val {
        PSQTi[PhaseMg][piece >> 1][sq],
        PSQTi[PhaseEg][piece >> 1][sq]
    };

    if (piece & 1) val = -val;

    return val;
}

void psqt_recalc()
{
    for (int phase : { PhaseMg, PhaseEg }) {
        for (int type = Pawn; type <= King; type++) {

            int val = MatValue[phase][type];
        
            for (int sq = 0; sq < 32; sq++) {
                if (type == Pawn && (sq <= 3 || sq >= 28)) continue;

                PSQTi[phase][type][sq] = val;
            }
        }
    }
}

void eval_init()
{
    for (int piece = WP12; piece <= BK12; piece++)
        for (int sq = 0; sq < 64; sq++)
            PSQTv[piece][sq] = reset_psqt(piece, sq);
}

template <size_t I>
string print_1d(string name, const int (&arr)[I])
{
    stringstream ss;

    ss << "int " << name << '[' << I << "] = {";

    for (size_t i = 0; i < I; i++) {
        if (i) ss << ',';

        ss << arr[i];
    }

    ss << "};" << endl;

    return ss.str();
}

template <size_t I, size_t J>
string print_2d(string name, const int (&arr)[I][J])
{
    stringstream ss;

    ss << "int " << name << '[' << I << "][" << J << "] = {" << endl;

    for (size_t i = 0; i < I; i++) {
        ss << "    {";

        for (size_t j = 0; j < J; j++) {
            if (j) ss << ',';

            ss << arr[i][j];
        }

        ss << '}';
        if (i != I - 1) ss << ',';
        ss << endl;
    }

    ss << "};" << endl;

    return ss.str();
}

template <size_t I, size_t J, size_t K>
string print_3d(string name, const int (&arr)[I][J][K])
{
    stringstream ss;

    ss << "int " << name << '[' << I << "][" << J << "][" << K << "] = {" << endl;

    for (size_t i = 0; i < I; i++) {
        ss << "    {" << endl;

        for (size_t j = 0; j < J; j++) {
            ss << "        {";

            for (size_t k = 0; k < K; k++) {
                if (k) ss << ',';

                ss << arr[i][j][k];
            }

            ss << '}';
            if (j != J - 1) ss << ',';
            ss << endl;
        }

        ss << "    }";
        if (i != I - 1) ss << ',';
        ss << endl;
    }

    ss << "};" << endl;

    return ss.str();
}

template <side_t Side>
int piece_on_file(const Position& pos, int orig)
{
    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);

    u64 fbb  = bb::Files[square::file(orig)];

    if (mpbb & fbb) return FileClosed;
    if (opbb & fbb) return FileSemiOpen;

    return FileOpen;
}

int eval_shelter(const Position& pos, side_t side, int ksq)
{
    int kfile = square::file(ksq);
    int krank = square::rank(ksq);

    // Adjust king position if on edge of board
   
    kfile = clamp(kfile, int(FileB), int(FileG));
    ksq = square::make(kfile, krank);

    int penalty = 0;

    u64 obb = pos.bb(side_t(!side));
    u64 pbb = pos.bb(side, Pawn);
    
    for (int orig : { ksq-1, ksq, ksq+1 }) {
        int edge = square::dist_edge(orig);

        u64 bb = bb::PawnSpan[side][orig] & (obb | pbb);
        
        int rank = Rank8;

        if (bb & pbb) {
            int sq = side == White ? bb::lsb(bb) : bb::msb(bb);

            rank = bb::test(obb, sq) ? Rank8 : square::rank(sq, side);
        }
        
        penalty += KingShelterP[edge][rank];
    }

    return penalty;
}

int eval_storm(const Position& pos, side_t side, int ksq)
{
    int kfile = square::file(ksq);
    int krank = square::rank(ksq);

    // Adjust king position if on edge of board
   
    kfile = clamp(kfile, int(FileB), int(FileG));
    ksq = square::make(kfile, krank);

    int penalty = 0;

    u64 pbb = pos.bb(side_t(!side), Pawn);
   
    for (int orig : { ksq-1, ksq, ksq+1 }) {
        int edge = square::dist_edge(orig);

        u64 bb = bb::PawnSpan[side][orig] & pbb;

        int rank = !bb ? Rank1 : square::rank(side == White ? bb::lsb(bb) : bb::msb(bb), side);
    
        penalty += KingStormP[edge][rank];
    }
    
    return penalty;
}


int eval_king(const Position& pos, side_t side)
{
    int ksq = pos.king(side);

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
Value eval_complexity(const Position& pos, int score)
{
    u64 pbb = pos.bb(Pawn);

    int sign    = (score > 0) - (score < 0);
    int pawns   = popcount(pbb);
    bool pieces = pos.pieces();
    bool flanks = (pbb & bb::QueenSide) && (pbb & bb::KingSide);

    int c = ComplexityPawnsTotal    * pawns
          + ComplexityPawnsFlanked  * flanks
          + ComplexityPawnsEndgame  * !pieces
          + ComplexityAdjustment;

    int v = sign * max(c, -abs(score));

    return Value(0, v);
}

// a la Ethereal
int eval_scale(const Position& pos, int score)
{
    u64 white   = pos.bb(White);
    u64 black   = pos.bb(Black);
    u64 pawns   = pos.bb(Pawn);
    u64 knights = pos.bb(Knight);
    u64 bishops = pos.bb(Bishop);
    u64 rooks   = pos.bb(Rook);
    u64 queens  = pos.bb(Queen);
    u64 pieces  = knights | bishops | rooks;

    // Opposite colored bishops

    if (bb::single(white & bishops) && bb::single(black & bishops) && bb::single(bishops & bb::Light)) {
        if (!(rooks | queens) && bb::single(white & knights) && bb::single(black & knights))
            return ScaleOcbOneKnight;

        if (!(knights | queens) && bb::single(white & rooks) && bb::single(black & rooks))
            return ScaleOcbOneRook;

        if (!(knights | rooks | queens))
            return ScaleOcbBishopsOnly;
    }

    // TODO Need a heuristic for bishop and pawn ending with pawn on A/H file

    u64 hi = score < 0 ? black : white;
    u64 lo = score < 0 ? white : black;

    int hpawns = popcount(hi & pawns);
    int lpawns = popcount(lo & pawns);

    if (bb::single(queens) && bb::multi(pieces) && pieces == (lo & pieces))
        return ScaleLoneQueen;

    if ((hi & (knights | bishops)) && bb::single(hi & ~pos.bb(King)))
        return 0;

    if (!queens && !bb::multi(white & pieces) && !bb::multi(black & pieces) && hpawns - lpawns > 2)
        return ScaleLargePawnAdv;

    if (hpawns == 0) return ScalePNone;
    if (hpawns == 1) return ScalePOne;
    if (hpawns == 2) return ScalePTwo;

    return 128;
}

template <side_t Side>
bool minor_on_outpost(const Position& pos, int orig)
{
    constexpr u64 subset = ~(bb::FileA | bb::FileH) & (Side == White ? bb::Rank4 | bb::Rank5 | bb::Rank6 : bb::Rank3 | bb::Rank4 | bb::Rank5);
    
    return bb::test(subset, orig)
        && (pos.bb(!Side, Pawn) & bb::PawnSpanAdj[Side][orig]) == 0
        && (pos.bb(Side, Pawn) & PawnAttacks[!Side][orig]);
}

template <side_t Side>
Value eval_pieces(const Position& pos)
{
    constexpr int incr = square::incr(Side);
    constexpr u64 outposts = ~(bb::FileA | bb::FileH) & (bb::Rank4 | bb::Rank5 | (Side == White ? bb::Rank6 : bb::Rank3));
    
    Value val;

    int mksq = pos.king(Side);
    int oksq = pos.king(!Side);

    int krank = square::rank(oksq, Side);

    u64 mpbb = pos.bb(Side, Pawn);
    u64 opbb = pos.bb(!Side, Pawn);

    u64 rank7 = Side == White ? bb::Rank7 : bb::Rank2;

    int npawns = popcount(mpbb);

    for (u64 bb = pos.pieces(Side); bb; ) {
        int orig = bb::pop(bb);
        int type = pos.board<6>(orig);

        int file     = square::file(orig);
        int rank_rel = square::rank(orig, Side);

        if (type != Queen) {
            int m_mg = PiecePawnOffset[PhaseMg][type];
            int m_eg = PiecePawnOffset[PhaseEg][type];

            val.mg += m_mg * (npawns - 4);
            val.eg += m_eg * (npawns - 4);
        }

        if (type == Knight) {
            if (bb::test(outposts, orig) && !(opbb & bb::PawnSpanAdj[Side][orig]) && (mpbb & PawnAttacks[!Side][orig]))
                val += Value(KnightOutpostBm, KnightOutpostBe);

            if (rank_rel < Rank7 && bb::test(mpbb, orig + incr))
                val += Value(MinorBehindPawnBm, MinorBehindPawnBe);

            int dist = square::dist(orig, mksq) * square::dist(orig, oksq);

            val.mg += -dist / KnightDistDivPm;
            val.eg += -dist / KnightDistDivPe;
        }

        else if (type == Bishop) {
            if (bb::test(outposts, orig) && !(opbb & bb::PawnSpanAdj[Side][orig]) && (mpbb & PawnAttacks[!Side][orig]))
                val += Value(BishopOutpostBm, BishopOutpostBe);

            if (rank_rel < Rank7 && bb::test(mpbb, orig + incr))
                val += Value(MinorBehindPawnBm, MinorBehindPawnBe);

            u64 mask = square::color(orig) ? bb::Dark : bb::Light;

            val += Value(BishopPawnsPm, BishopPawnsPe) * popcount(mpbb & mask);
        }

        else if (type == Rook) {
            int kfile    = square::file(oksq);
            int kfdiff   = abs(file - kfile);
            int semiopen = piece_on_file<Side>(pos, orig);

            if (rank_rel == Rank7 && (krank == Rank8 || (opbb & rank7)))
                val += Value(RookSeventhRankBm, RookSeventhRankBe);

            if (semiopen == FileClosed)
                val += Value(RookClosedPm, RookClosedPe);
            else if (kfdiff == 0) {
                val.mg += semiopen == FileSemiOpen ? RookSemiSameKingBm : RookOpenSameKingBm;
                val.eg += semiopen == FileSemiOpen ? RookSemiSameKingBe : RookOpenSameKingBe;
            }
            else if (kfdiff == 1) {
                val.mg += semiopen == FileSemiOpen ? RookSemiAdjKingBm : RookOpenAdjKingBm;
                val.eg += semiopen == FileSemiOpen ? RookSemiAdjKingBe : RookOpenAdjKingBe;
            }
            else {
                val.mg += semiopen == FileSemiOpen ? RookSemiBm : RookOpenBm;
                val.eg += semiopen == FileSemiOpen ? RookSemiBe : RookOpenBe;
            }
        }

        else if (type == Queen) {
            if (rank_rel == Rank7 && (krank == Rank8 || (opbb & rank7))) {
                val.mg += QueenSeventhRankBm;
                val.eg += QueenSeventhRankBe;
            }
        }
    }
    
    return val;
}

// TODO refactor
template <side_t Side>
Value eval_imbalance(const Position& pos)
{
    Value val;

    int mp, mn, mb, mr, mq;
    int op, on, ob, orr, oq;

    if (Side == White)
        pos.counts(mp, mn, mb, mr, mq, op, on, ob, orr, oq);
    else
        pos.counts(op, on, ob, orr, oq, mp, mn, mb, mr, mq);

    int nd = mn - on;
    int bd = mb - ob;
    int rd = mr - orr;
    int qd = mq - oq;

    // Up rook

    if (rd == 1 && qd == 0) {
        if (nd == -1 && bd == 0) {
            val.mg += RookKnightImbB[PhaseMg];
            val.eg += RookKnightImbB[PhaseEg];
        }

        else if (nd == 0 && bd == -1) {
            val.mg += RookBishopImbB[PhaseMg];
            val.eg += RookBishopImbB[PhaseEg];
        }

        else if ((mn + mb) - (on + ob) == -2) {
            val.mg += RookTwoMinorsImbB[PhaseMg];
            val.eg += RookTwoMinorsImbB[PhaseEg];
        }
    }

    // Up queen

    else if (qd == 1) {
        if (rd == -2 && nd == 0 && bd == 0) {
            val.mg += QueenTwoRooksImbB[PhaseMg];
            val.eg += QueenTwoRooksImbB[PhaseEg];
        }

        else if (rd == -1 && nd == -1 && bd == 0) {
            val.mg += QueenRookKnightImbB[PhaseMg];
            val.eg += QueenRookKnightImbB[PhaseEg];
        }

        else if (rd == -1 && nd == 0 && bd == -1) {
            val.mg += QueenRookBishopImbB[PhaseMg];
            val.eg += QueenRookBishopImbB[PhaseEg];
        }
    }

    return val;
}

Value eval_mob_ks(const Position& pos)
{
    Value sval[2];

    int ks_factors[4] = {
        KingSafetyA,
        KingSafetyB,
        KingSafetyC,
        KingSafetyD
    };

    u64 occ = pos.occ();

    for (side_t side : { White, Black }) {
        u64 mrbb = pos.bb(!side, Rook);
        u64 oqbb = pos.bb(side, Queen);
        u64 mqbb = pos.bb(!side, Queen);

        u64 targets = ~(pos.bb(side_t(!side)) | bb::PawnAttacks(pos.bb(side, Pawn), side));

        int ksq = pos.king(side);
        int ks_weight = 0;
        int ks_attackers = 0;
        
        u64 kzone = KingZone[ksq];

        for (u64 bb = pos.bb(!side, Knight); bb; ) {
            int sq = bb::pop(bb);

            u64 amask = bb::Leorik::Knight(sq);
            u64 att = amask & targets;
        
            int mob = popcount(att);

            sval[!side].mg += MobB[PhaseMg][Knight-1][LOG[mob]];
            sval[!side].eg += MobB[PhaseEg][Knight-1][LOG[mob]];
            
            if (amask & oqbb)
                sval[!side].mg += NQAttBm;
            
            if (amask & mqbb) {
                sval[!side].mg += NQDefBm;
                sval[!side].eg += NQDefBe;
            }

            if (att & kzone) {
                ks_weight += KingSafetyW[0][square::dist(ksq, sq)];
                ++ks_attackers;
            }
        }
    
        for (u64 bb = pos.bb(!side, Bishop); bb; ) {
            int sq = bb::pop(bb);

            u64 amask = bb::Leorik::Bishop(sq, occ);
            u64 att = amask & targets;

            int mob = popcount(att);

            sval[!side].mg += MobB[PhaseMg][Bishop-1][LOG[mob]];
            sval[!side].eg += MobB[PhaseEg][Bishop-1][LOG[mob]];

            if (amask & oqbb) {
                sval[!side].mg += BQAttBm;
                sval[!side].eg += BQAttBe;
            }
            
            if (amask & mqbb) {
                sval[!side].mg += BQDefBm;
                sval[!side].eg += BQDefBe;
            }
            
            if (att & kzone) {
                ks_weight += KingSafetyW[1][square::dist(ksq, sq)];
                ++ks_attackers;
            }
        }
       
        for (u64 bb = mrbb; bb; ) {
            int sq = bb::pop(bb);

            u64 amask = bb::Leorik::Rook(sq, occ);
            u64 att = amask & targets;

            int mob = popcount(att);

            sval[!side].mg += MobB[PhaseMg][Rook-1][LOG[mob]];
            sval[!side].eg += MobB[PhaseEg][Rook-1][LOG[mob]];

            if (amask & oqbb)
                sval[!side].mg += RQAttBm;

            u64 fmask = bb::Files[square::file(sq)];
            u64 rmask = bb::Ranks[square::rank(sq)];

            if (fmask & amask & mrbb) {
                sval[!side].mg += RRSameFileBm;
                sval[!side].eg += RRSameFileBe;
            }
            
            if (rmask & amask & mrbb) {
                sval[!side].mg += RRSameRankBm;
                sval[!side].eg += RRSameRankBe;
            }
           
            if (fmask & amask & mqbb) {
                sval[!side].mg += RQSameFileBm;
                sval[!side].eg += RQSameFileBe;
            }
            
            if (rmask & amask & mqbb) {
                sval[!side].mg += RQSameRankBm;
                sval[!side].eg += RQSameRankBe;
            }

            if (att & kzone) {
                ks_weight += KingSafetyW[2][square::dist(ksq, sq)];
                ++ks_attackers;
            }
        }

        for (u64 bb = mqbb; bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Queen(sq, occ) & targets;

            int mob = popcount(att);

            sval[!side].mg += MobB[PhaseMg][Queen-1][LOG[mob]];
            sval[!side].eg += MobB[PhaseEg][Queen-1][LOG[mob]];

            if (att & kzone) {
                ks_weight += KingSafetyW[3][square::dist(ksq, sq)];
                ++ks_attackers;
            }
        }

        if (pos.phase(!side) >= 4) {
            if (ks_attackers > 0) {
                ks_attackers = min(ks_attackers - 1, 3);

                sval[side].mg -= (ks_weight * ks_factors[ks_attackers] + 50) / 100;
            }

            sval[side].mg += eval_king(pos, side);
        }
    }

    return sval[White] - sval[Black];
}

Value eval_pattern(const Position& pos)
{
    Value val;

    u64 brbb = pos.bb(Black, Rook);
    u64 bkbb = pos.bb(Black, King);
    u64 wrbb = pos.bb(White, Rook);
    u64 wkbb = pos.bb(White, King);

    // White

    constexpr u64 wqs_rmask = bb::bit(square::A1) | bb::bit(square::A2) | bb::bit(square::B1);
    constexpr u64 wqs_kmask = bb::bit(square::B1) | bb::bit(square::C1);
    constexpr u64 wks_rmask = bb::bit(square::H1) | bb::bit(square::H2) | bb::bit(square::G1);
    constexpr u64 wks_kmask = bb::bit(square::F1) | bb::bit(square::G1);

    if ((wrbb & wqs_rmask) && (wqs_kmask & wkbb)) val += Value(RBlockedPm, RBlockedPe);
    if ((wrbb & wks_rmask) && (wks_kmask & wkbb)) val += Value(RBlockedPm, RBlockedPe);

    // Black

    constexpr u64 bqs_rmask = bb::bit(square::A7) | bb::bit(square::A8) | bb::bit(square::B8);
    constexpr u64 bqs_kmask = bb::bit(square::B8) | bb::bit(square::C8);
    constexpr u64 bks_rmask = bb::bit(square::H7) | bb::bit(square::H8) | bb::bit(square::G8);
    constexpr u64 bks_kmask = bb::bit(square::F8) | bb::bit(square::G8);

    if ((brbb & bqs_rmask) && (bqs_kmask & bkbb)) val -= Value(RBlockedPm, RBlockedPe);
    if ((brbb & bks_rmask) && (bks_kmask & bkbb)) val -= Value(RBlockedPm, RBlockedPe);

    return val;
}

// TODO refactor with pos.pins()
template <side_t Side>
Value eval_pins(const Position& pos)
{
    Value val;

    u64 occ = pos.occ();
    u64 mnbb = pos.bb(Side, Knight);
    u64 mbbb = pos.bb(Side, Bishop);
    u64 mrbb = pos.bb(Side, Rook);
    u64 mqbb = pos.bb(Side, Queen);

    u64 obbb = pos.bb(!Side, Bishop);
    u64 orbb = pos.bb(!Side, Rook);
    u64 oqbb = pos.bb(!Side, Queen);

    u64 blockers = ~(mnbb | mbbb | mrbb | mqbb);

    int ksq = pos.king(Side);

    for (u64 bb = BishopAttacks[ksq] & (obbb | oqbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[ksq][sq] & occ;

        if ((between & blockers) || !bb::single(between))
            continue;

        int pinner = bb::test(obbb, sq) ? Bishop : Queen;

        // Absolute pins

        if (between & mnbb) {
            if (pinner == Bishop) {
                val.mg += NPinByBAbsPm;
                val.eg += NPinByBAbsPe;
            }

            else if (pinner == Queen) {
                val.mg += NPinByQAbsPm;
                val.eg += NPinByQAbsPe;
            }
        }

        else if (between & mrbb) {
            if (pinner == Bishop) {
                val.mg += RPinByBAbsPm;
                val.eg += RPinByBAbsPe;
            }

            else if (pinner == Queen) {
                val.mg += RPinByQAbsPm;
                val.eg += RPinByQAbsPe;
            }
        }

        // Relative pins

        else if (between & mqbb) {
            if (pinner == Bishop) {
                val.mg += QPinByBRelPm;
                val.eg += QPinByBRelPe;
            }
        }
    }
    
    for (u64 bb = RookAttacks[ksq] & (orbb | oqbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[ksq][sq] & occ;

        if ((between & blockers) || !bb::single(between))
            continue;

        int pinner = bb::test(orbb, sq) ? Rook : Queen;

        // Absolute pins

        if (between & mnbb) {
            if (pinner == Rook) {
                val.mg += NPinByRAbsPm;
                val.eg += NPinByRAbsPe;
            }

            else if (pinner == Queen) {
                val.mg += NPinByQAbsPm;
                val.eg += NPinByQAbsPe;
            }
        }

        else if (between & mbbb) {
            if (pinner == Rook) {
                val.mg += BPinByRAbsPm;
                val.eg += BPinByRAbsPe;
            }

            else if (pinner == Queen) {
                val.mg += BPinByQAbsPm;
                val.eg += BPinByQAbsPe;
            }
        }

        // Relative pins

        else if (between & mqbb) {
            if (pinner == Rook) {
                val.mg += QPinByRRelPm;
                val.eg += QPinByRRelPe;
            }
        }
    }

    return val;
}

int eval_internal(const Position& pos)
{
    assert(!pos.checkers());

#ifndef TUNE
    EvalEntry eentry;

    if (etable.get(pos.key(), eentry))
        return eentry.score + TempoB;
#endif

    Value val;

    // PSQT

    for (u64 bb = pos.occ(); bb; ) {
        int sq = bb::pop(bb);
        int p12 = pos.board<12>(sq);

        val += PSQTv[p12][sq];
    }

    // Bishop pair

    int wbp = pos.bishop_pair(White);
    int bbp = pos.bishop_pair(Black);

    val += (wbp - bbp) * Value(BPairBm, BPairBe);

    // Material imbalances

    val += eval_imbalance<White>(pos);
    val -= eval_imbalance<Black>(pos);

    // Mobility and King Safety

    val += eval_mob_ks(pos);

    // Pinned pieces

    val += eval_pins<White>(pos);
    val -= eval_pins<Black>(pos);

    // Patterns

    val += eval_pattern(pos);

    // Major and minor pieces

    val += eval_pieces<White>(pos);
    val -= eval_pieces<Black>(pos);

    if (pos.bb(Pawn)) {
        //assert(pos.pawn_key());

        val += eval_pawns(pos);
    }

    val += eval_complexity(pos, val.eg);

    int score = val.lerp(pos.phase(), eval_scale(pos, val.eg));

    if (pos.side() == Black) score = -score;

#ifndef TUNE
    eentry.score = score;

    etable.set(pos.key(), eentry);
#endif
    
    return score + TempoB;
}

int eval(const Position& pos)
{
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

#ifdef TUNE

#include <nlopt.hpp>
#include <omp.h>

enum Tuning {
    TuneComplexity = 1 <<  0,
    TuneKingSafety = 1 <<  1,
    TuneBishopPair = 1 <<  2,
    TuneMobility   = 1 <<  3,
    TunePattern    = 1 <<  4,
    TunePawns      = 1 <<  5,
    TunePieces     = 1 <<  6,
    TunePinned     = 1 <<  7,
    TunePsqt       = 1 <<  8,
    TuneScale      = 1 <<  9,
    TuneTempo      = 1 << 10,
    TuneMaterial   = 1 << 11,
    TuneImbalance  = 1 << 12,
    TuneAll        = ~0
};

static constexpr u64 TuneCurrent    = 0
                                    //| TuneMaterial
                                    //| TuneKingSafety
                                    //| TuneBishopPair
                                    //| TuneTempo
                                    //| TuneMobility
                                    //| TunePattern
                                    //| TunePawns
                                    //| TunePieces
                                    //| TunePsqt
                                    //| TuneScale
                                    //| TuneComplexity
                                    | TunePinned
                                    //| TuneImbalance
                                    ;

static double KOptimal = 0.788745;

mt19937_64 mt;

struct Tuner {
    double maxtime  = 0;
    size_t seed     = 0;
    size_t threads  = omp_get_max_threads();
    double ratio    = 1;

    bool bench      = false;
    bool debug      = false;
    bool kopt       = false;
    bool local      = false;
    bool opt        = false;
    bool randinit   = false;

    filesystem::path path { "tuner.bin" };
    
    string str() const
    {
        stringstream ss;

        ss << "Tuner," << maxtime << ',' << seed << ',' << threads << ',' << ratio << ','
           << bench << ',' << debug << ',' << kopt << ',' << local << ','
           << opt << ',' << randinit;

        return ss.str();
    }
};

Tuner tuner;

string format(const char format[], ...)
{
    va_list arg_list;
    char buf[64];

    va_start(arg_list, format);
    vsprintf(buf, format, arg_list);
    va_end(arg_list);

    return buf;
}

struct Param {
    string name;
    int * pvalue;
    int init;
    int min;
    int max;
    int start;
    
    Param(string n, int * p, int min_, int max_)
    {
        name = n;
        pvalue = p;

        min = min_;
        max = max_;

        start = *p;

        if (start < min || start > max) {
            start = *p = clamp(start, min, max);

            cerr << "Starting value for " << n << " is out of range!" << endl;

            //exit(EXIT_FAILURE);
        }

        if (tuner.randinit) {
            uniform_int_distribution<int> gen(min, max);
            init = gen(mt);
        }
        else
            init = *p;
    }
    
    //Param(string n, int * p, int delta) : Param(n, p, *p - delta, *p + delta) { }

    string str() const
    {
        stringstream ss;

        ss << "Param," << name << ',' << min << ',' << start << ',' 
           << max << ',' << *pvalue << ',' << init;

        return ss.str();
    }

    int bound() const
    {
        int margin = std::max((max - min) / 20, 1);

        if (*pvalue <= min + margin) return -1;
        if (*pvalue >= max - margin) return  1;

        return 0;
    }
};

vector<Position> g_pos;
vector<Param> g_params;
vector<Param> g_params_orig;

double sigmoid(int e, double K)
{
    return 1 / (1 + exp(-K * e / 400));
}

double eval_error(const Position& pos, double K, int * cp = nullptr)
{
    int e = eval(pos);

    if (pos.side() == Black) e = -e;

    if (cp) *cp = e;

    return pow(sigmoid(pos.score1(), 1) - sigmoid(e, K), 2);
}

double eval_error(const vector<Position>& v, double K)
{
    double sse = 0;

    omp_set_num_threads(tuner.threads);
    
    const size_t N = v.size() / tuner.threads;
   
    #pragma omp parallel
    {
        const size_t id = omp_get_thread_num();

        double se = 0;

        for (size_t i = id * N; i < (id + 1) * N; i++)
            se += eval_error(v[i], K);

        #pragma omp critical
        sse += se;
    }

    for (size_t i = tuner.threads * N; i < v.size(); i++)
        sse += eval_error(v[i], K);

    return sse / v.size();
}

void eval_read_epd(vector<Position>& v)
{
    g_pos.reserve(size_t(5000000 * tuner.ratio));

    cout << "Reading tuning file... " << flush;

    string line;

    ifstream ifs(tuner.path);

    uniform_real_distribution<> dist(0.0, 1.0);

    Timer timer(true);

    while (getline(ifs, line)) {
        assert(!line.empty());

        if (tuner.ratio < 1.0 && dist(mt) > tuner.ratio) continue;
#if 0
        Tokenizer tokens(line, ',');

        const string& fen = tokens[0];
        const string& sc1 = tokens[1];
        const string& sc2 = tokens[2];

        int score1 = atoi(sc1.c_str());
        int score2 = atoi(sc2.c_str());

        v.emplace_back(fen, score1, score2);
#else
        size_t pos = line.find('[');
        string fen = line.substr(0, pos - 1);
        string score1 = line.substr(pos + 1, 3);
        string score2 = line.substr(pos + 6);

        int sc1 = score1 == "1.0" ? 2 : (score1 == "0.0" ? 0 : 1);
        int sc2 = atoi(score2.c_str());

        if (abs(sc2) >= 1000) continue;

        Position pstn(fen, sc1, 0);

        if (!pstn.checkers())
            v.push_back(pstn);

#endif
    }

    timer.stop();

    double dur = timer.elapsed_time() / 1000.0;

    cout << "OK"
         << endl
         << "Parsed " << v.size() << " positions in " 
         << dur << " seconds, " 
         << size_t(v.size() / dur / 1000) << " kps" 
         << endl;

#if 0
    ofstream ofs("tuner.bin", ios::binary);

    for (Position& pos : v) {
        if (!pos.checkers()) {
            PositionBin bin = pos.serialize();
            ofs.write(bin.get(), sizeof(PositionBin));
        }
    }
#endif
}

void eval_read_bin(vector<Position>& v)
{
    g_pos.reserve(size_t(5000000 * tuner.ratio));

    cout << "Reading tuning file... " << flush;

    auto vec = mem::read(tuner.path);

    uniform_real_distribution<> dist(0.0, 1.0);

    PositionBin bin;

    Timer timer(true);

    for (size_t i = 0; i < vec.size(); i += sizeof(PositionBin)) {
        if (tuner.ratio < 1.0 && dist(mt) > tuner.ratio) continue;
        
        memcpy(&bin, &vec[i], sizeof(bin));

        v.emplace_back(bin);
    }

    timer.stop();

    double dur = timer.elapsed_time() / 1000.0;

    cout << "OK"
         << endl
         << "Parsed " << v.size() << " positions in " 
         << dur << " seconds, " 
         << size_t(v.size() / dur / 1000) << " kps" 
         << endl;

#if 0
    int counts4[16] = { };

    for (u8 byte : vec) {
        ++counts4[(byte >> 0) & 15];
        ++counts4[(byte >> 4) & 15];
    }
    
    for (int i = 0; i < 16; i++)
        cout << "n," << i << ",counts4," << counts4[i] << endl;

    cout << endl;

    int counts8[256] = { };

    for (u8 byte : vec)
        ++counts8[byte];
    
    for (int i = 0; i < 256; i++)
        cout << "n," << i << ",counts8," << counts8[i] << endl;

    exit(EXIT_SUCCESS);
#endif
}

double myvfunc(const vector<double>& x, 
               [[maybe_unused]] vector<double>& grad, 
               [[maybe_unused]] void * my_func_data)
{
    static int iter     = 1;
    static int btime    = time(nullptr);
    static double minf  = numeric_limits<double>::max();
    static double maxf  = 1;

    const int dur = max(int(time(nullptr) - btime), 1);

    for (size_t i = 0; i < x.size(); i++) {
        int n = clamp(int(x[i]), g_params[i].min, g_params[i].max);

        *g_params[i].pvalue = n;
    }

    if ((TuneCurrent & TuneMaterial) && (TuneCurrent & TunePsqt)) {
        cerr << "Cannot tune both material and psqt tables!" << endl;
        exit(EXIT_FAILURE);
    }

    if (TuneCurrent & TuneMaterial) {
        psqt_recalc();
        eval_init();
    }

    if (TuneCurrent & TunePsqt)
        eval_init();
   
    double f = eval_error(g_pos, KOptimal);

    if (iter == 1) maxf = f;

    if (f < minf) {
        minf = f;

        stringstream ss;

        for (auto &param : g_params_orig) {
            bool array = param.name.find('[') != string::npos;

            int p1 = *param.pvalue;
            int p2 = param.start;
            int diff = p1 - p2;
            int bound = param.bound();
            string info;

            if (bound) info = format(" %s : %i / %i", bound < 0 ? "----" : "++++", param.min, param.max);

            if (!array || bound)
            ss << "int "
               << setw(32) << setfill(' ') << left << param.name << '='
               << setw(8) << setfill(' ') << right << *param.pvalue << "; diff = "
               << setw(8) << setfill(' ') << diff << info << endl;
        }

        if (TuneCurrent & TuneMobility)
            ss << print_3d<2, 4, 8>("MobB", MobB) << endl;

        if (TuneCurrent & TunePsqt)
            ss << print_3d<2, 6, 32>("PSQTi", PSQTi) << endl;

        if (TuneCurrent & TuneMaterial)
            ss << print_2d<2, 6>("MatValue", MatValue) << endl;

        if (TuneCurrent & TunePawns) {
            ss << print_1d<8>("PawnCandidateB", PawnCandidateB) << endl;
            ss << print_1d<8>("PawnConnectedB", PawnConnectedB) << endl;
            ss << print_3d<2, 3, 8>("PawnPassedB", PawnPassedB) << endl;
            ss << print_3d<2, 2, 6>("PawnAttackB", PawnAttackB) << endl;
            ss << print_3d<2, 2, 6>("PawnAttackPushB", PawnAttackPushB) << endl;
        }

        if (TuneCurrent & TuneKingSafety) {
            ss << print_2d<4, 8>("KingShelterP", KingShelterP) << endl;
            ss << print_2d<4, 8>("KingStormP", KingStormP) << endl;
            ss << print_2d<4, 8>("KingSafetyW", KingSafetyW) << endl;
        }

        if (TuneCurrent & TunePieces)
            ss << print_2d<2, 6>("PiecePawnOffset", PiecePawnOffset) << endl;

        if (TuneCurrent & TuneImbalance) {
            ss << print_1d<2>("RookKnightImbB", RookKnightImbB) << endl;
            ss << print_1d<2>("RookBishopImbB", RookBishopImbB) << endl;
            ss << print_1d<2>("RookTwoMinorsImbB", RookTwoMinorsImbB) << endl;
            ss << print_1d<2>("QueenTwoRooksImbB", QueenTwoRooksImbB) << endl;
            ss << print_1d<2>("QueenRookKnightImbB", QueenRookKnightImbB) << endl;
            ss << print_1d<2>("QueenRookBishopImbB", QueenRookBishopImbB) << endl;
        }

        char buf[32];

        {
            int n = dur;
            int days = dur / 24 / 60 / 60;
            n %= 24 * 60 * 60;
            int hours = n / 60 / 60;
            n %= 60 * 60;
            int minutes = n / 60;
            n %= 60;
            int seconds = n;

            snprintf(buf, sizeof(buf), "%i:%02i:%02i:%02i", days, hours, minutes, seconds);
        }

        ss << "mse=" << fixed << setprecision(7) << minf
           << ",rmse=" << sqrt(minf)
           << ",ai=" << maxf - minf
           << ",ri=" << (maxf - minf) / maxf * 100
           << ",i=" << iter
           << ",t=" << dur
           << ",d=" << buf
           << ",is=" << setprecision(2) << double(iter) / dur
           << ",si=" << setprecision(2) << dur / double(iter)
           << ",p=" << x.size() << endl;

        cout << ss.str();

        fstream fs("opt.txt", ios::out | ios::app);
        fs << ss.str();
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
double eval_K(double lb, double ub, double tol, double& mse, size_t& iter)
{
    const double invphi  = (sqrt(5) - 1) / 2;
    const double invphi2 = (3 - sqrt(5)) / 2;

    double h = ub - lb;

    double c = lb + invphi2 * h;
    double yc = eval_error(g_pos, c);
    double d = lb + invphi  * h;
    double yd = eval_error(g_pos, d);

    size_t n = std::ceil(log(tol / h) / log(invphi));
    
    iter = 2 + n;

    for (size_t i = 0; i < n; i++) {
        // yc > yd to find the maximum
        if (yc < yd) {
            ub = d;
            d = c;
            yd = yc;
            h = invphi * h;
            c = lb + invphi2 * h;
            yc = eval_error(g_pos, c);
        }
        else {
            lb = c;
            c = d;
            yc = yd;
            h = invphi * h;
            d = lb + invphi * h;
            yd = eval_error(g_pos, d);
        }
    }

    mse = (yc + yd) / 2;

    return (yc < yd ? lb + d : ub + c) / 2;
}

void eval_tune(int argc, char* argv[])
{
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
        else if (k == "kopt")
            tuner.kopt = true;
        else if (k == "local")
            tuner.local = true;
        else if (k == "opt")
            tuner.opt = true;
        else if (k == "randinit")
            tuner.randinit = true;
        else if (k == "ratio")
            tuner.ratio = stoull(v) / 100.0;
        else if (k == "seed")
            tuner.seed = stoull(v);
        else if (k == "threads")
            tuner.threads = stoull(v);
        else if (k == "maxtime")
            tuner.maxtime = stod(v);
    }

    if (tuner.seed)
        mt = mt19937_64(tuner.seed);
    else
        mt = mt19937_64(random_device{}());
            
    if (!filesystem::exists(tuner.path)) {
        cout << "Input file does not exist!" << endl;
        exit(EXIT_FAILURE);
    }

    string ext = tuner.path.extension();

    if (ext == ".epd")
        eval_read_epd(g_pos);
    else if (ext == ".bin")
        eval_read_bin(g_pos);
    else {
        cerr << "Input file not recognized!" << endl;
        exit(EXIT_FAILURE);
    }


    if (tuner.bench) {
        Timer timer(true);
        double error = eval_error(g_pos, KOptimal);
        cout << error << ',' << sqrt(error) << endl;
        timer.stop();

        i64 dur = timer.elapsed_time();

        cout << dur << " ms" << endl;
        cout << g_pos.size() / dur << " ke/s" << endl;
    }
    
    if (tuner.kopt) {
        size_t iter;
        double mse;
        double K = eval_K(0, 5, 1e-5, mse, iter);

        cout << "Current K = " << KOptimal
             << ", Optimal K = " << K
             << ", MSE = " << mse
             << ", Iterations = " << iter << endl;

        KOptimal = K;
    }

    if (!tuner.opt && !tuner.debug)
        return;

#define MP(var) MPS(var), &var
#define MPS(var) #var
#define GPPB g_params.push_back
   
#if 0
    if (TuneCurrent & TuneMaterial) {
        for (int phase : { PhaseMg, PhaseEg }) {
            for (int type = Knight; type < King; type++) {
                string name = format("MatValue[%i][%i]", phase, type);

                int val = MatValue[phase][type];
                int delta = val / 4;

                int lb = val - delta;
                int ub = val + delta;

                GPPB(Param(name, &MatValue[phase][type], lb, ub));
            }
        }
    }
#endif

    if (TuneCurrent & TunePsqt) {
        for (int phase : { PhaseMg, PhaseEg }) {
            for (int type = Pawn; type <= King; type++) {

                int start = MatValue[phase][type];
                int delta = type == King ? 200 : start / 4;
            
                for (int sq = 0; sq < 32; sq++) {
                    if (type == Pawn && (sq <= 3 || sq >= 28)) continue;

                    int lb = start - delta;
                    int ub = start + delta;

                    // Pawns on advanced ranks are worth much more
                    if (type == Pawn && sq >= 16)
                        ub += delta * 3;

                    if (type != King)
                        lb -= lb / 4;

                    // Knights on corner squares are worth much less
                    if (type == Knight && (sq == 0 || sq == 28))
                        lb = 0;

                    GPPB(Param(format("PSQTi[%i][%i][%i]", phase, type, sq), &PSQTi[phase][type][sq], lb, ub));
                }
            }
        }
    }
    
    if (TuneCurrent & TuneBishopPair) {
        GPPB(Param(MP(BPairBm),  0,  50));
        GPPB(Param(MP(BPairBe), 50, 100));
    }

    if (TuneCurrent & TunePattern) {
        GPPB(Param(MP(RBlockedPm), -200, 0));
        GPPB(Param(MP(RBlockedPe), -200, 0));
    }
   
    if (TuneCurrent & TuneMobility) {
        GPPB(Param(MP(NQAttBm),        0, 150));
        GPPB(Param(MP(BQAttBm),        0, 150));
        GPPB(Param(MP(BQAttBe),        0, 150));
        GPPB(Param(MP(RQAttBm),      -10, 100));
        GPPB(Param(MP(NQDefBm),      -10, 100));
        GPPB(Param(MP(NQDefBe),      -10, 100));
        GPPB(Param(MP(BQDefBm),      -10, 100));
        GPPB(Param(MP(BQDefBe),      -10, 100));
        GPPB(Param(MP(RRSameFileBm),  -5,  50));
        GPPB(Param(MP(RRSameFileBe),  -5,  50));
        GPPB(Param(MP(RRSameRankBm),  -5,  50));
        GPPB(Param(MP(RRSameRankBe),  -5,  50));
        GPPB(Param(MP(RQSameFileBm),  -5,  50));
        GPPB(Param(MP(RQSameFileBe),  -5,  50));
        GPPB(Param(MP(RQSameRankBm),  -5,  50));
        GPPB(Param(MP(RQSameRankBe),  -5,  50));
       
        for (int phase : { PhaseMg, PhaseEg }) {
            for (int type = Knight; type <= Queen; type++) {
                int log[4] = { 6, 7, 7, 8 };

                for (int i = 0; i < log[type - 1]; i++) {
                    int delta = MatValue[phase][type] / (phase == PhaseMg ? 4 : 2);

                    string name = format("MobB[%i][%i][%i]", phase, type - 1, i);
            
                    GPPB(Param(name, &MobB[phase][type - 1][i], -delta, delta));
                }
            }
        }
    }
  
    if (TuneCurrent & TuneKingSafety) {
        GPPB(Param(MP(KingSafetyA),  1,  97));
        GPPB(Param(MP(KingSafetyB),  2,  98));
        GPPB(Param(MP(KingSafetyC),  3,  99));
        GPPB(Param(MP(KingSafetyD),  4, 100));

        for (int type = Knight; type <= Queen; type++) {
            for (int dist = 0; dist < 8; dist++) {
                if (KingSafetyW[type - 1][dist] == 0) continue;

                int ub = type == Queen && dist == 2 ? 1000 : 500;

                string name = format("KingSafetyW[%i][%i]", type - 1, dist);

                GPPB(Param(name, &KingSafetyW[type - 1][dist], 1, ub));
            }
        }
       
        for (int i = 0; i <= 3; i++) {
            for (int rank = Rank3; rank <= Rank5; rank++) {
                string name = format("KingStormP[%i][%i]", i, rank);

                int lb = -125 + 50 * (rank - 2);

                GPPB(Param(name, &KingStormP[i][rank], lb, 0));
            }
        }
        
        for (int i = 0; i <= 3; i++) {
            for (int rank = Rank3; rank <= Rank8; rank++) {
                string name = format("KingShelterP[%i][%i]", i, rank);

                int lb = -25 * (rank - 1);

                GPPB(Param(name, &KingShelterP[i][rank], lb, 0));
            }
        }
    }
    
    if (TuneCurrent & TunePinned) {
        GPPB(Param(MP(NPinByBAbsPm), -1000, 1000));
        GPPB(Param(MP(NPinByBAbsPe), -1000, 1000));
        GPPB(Param(MP(NPinByRAbsPm), -1000, 1000));
        GPPB(Param(MP(NPinByRAbsPe), -1000, 1000));
        GPPB(Param(MP(NPinByQAbsPm), -1000, 1000));
        GPPB(Param(MP(NPinByQAbsPe), -1000, 1000));
        GPPB(Param(MP(BPinByRAbsPm), -1000, 1000));
        GPPB(Param(MP(BPinByRAbsPe), -1000, 1000));
        GPPB(Param(MP(BPinByQAbsPm), -1000, 1000));
        GPPB(Param(MP(BPinByQAbsPe), -1000, 1000));
        GPPB(Param(MP(RPinByBAbsPm), -1000, 1000));
        GPPB(Param(MP(RPinByBAbsPe), -1000, 1000));
        GPPB(Param(MP(RPinByQAbsPm), -1000, 1000));
        GPPB(Param(MP(RPinByQAbsPe), -1000, 1000));
        GPPB(Param(MP(QPinByBRelPm), -1000, 1000));
        GPPB(Param(MP(QPinByBRelPe), -1000, 1000));
        GPPB(Param(MP(QPinByRRelPm), -1000, 1000));
        GPPB(Param(MP(QPinByRRelPe), -1000, 1000));
    }
  
    if (TuneCurrent & TunePieces) {
        GPPB(Param(MP(MinorBehindPawnBm),     1,  50));
        GPPB(Param(MP(MinorBehindPawnBe),     1,  50));
        GPPB(Param(MP(KnightOutpostBm),       1,  50));
        GPPB(Param(MP(KnightOutpostBe),       1,  50));
        GPPB(Param(MP(KnightDistDivPm),       1,  25));
        GPPB(Param(MP(KnightDistDivPe),       1,  25));

        GPPB(Param(MP(BishopOutpostBm),       1,  50));
        GPPB(Param(MP(BishopOutpostBe),       1,  50));
        GPPB(Param(MP(BishopPawnsPm),       -10,   0));
        GPPB(Param(MP(BishopPawnsPe),       -10,   0));

        GPPB(Param(MP(RookClosedPm),        -75,  75));
        GPPB(Param(MP(RookClosedPe),        -75,  75));
        GPPB(Param(MP(RookOpenAdjKingBm),   -10, 100));
        GPPB(Param(MP(RookOpenAdjKingBe),   -10, 100));
        GPPB(Param(MP(RookOpenBm),          -10, 100));
        GPPB(Param(MP(RookOpenBe),          -10, 100));
        GPPB(Param(MP(RookOpenSameKingBm),  -20, 200));
        GPPB(Param(MP(RookOpenSameKingBe),  -20, 200));
        GPPB(Param(MP(RookSemiAdjKingBm),   -10, 100));
        GPPB(Param(MP(RookSemiAdjKingBe),   -10, 100));
        GPPB(Param(MP(RookSemiBm),          -50, 100));
        GPPB(Param(MP(RookSemiBe),          -50, 100));
        GPPB(Param(MP(RookSemiSameKingBm),  -10, 100));
        GPPB(Param(MP(RookSemiSameKingBe),  -10, 100));
        GPPB(Param(MP(RookSeventhRankBm),   -50, 100));
        GPPB(Param(MP(RookSeventhRankBe),   -50, 100));

        GPPB(Param(MP(QueenSeventhRankBm), -100, 200));
        GPPB(Param(MP(QueenSeventhRankBe), -100, 200));
       
        for (int phase : { PhaseMg, PhaseEg }) {
            for (int type = Knight; type <= Rook; type++) {
                string name = format("PiecePawnOffset[%i][%i]", phase, type);
                
                int delta = MatValue[phase][type] / 25;
            
                GPPB(Param(name, &PiecePawnOffset[phase][type], -delta, delta));
            }
        }
    }
 
    if (TuneCurrent & TunePawns) {
        GPPB(Param(MP(PawnBackOpenPm), -50,   0));
        GPPB(Param(MP(PawnBackOpenPe), -50,   0));
        GPPB(Param(MP(PawnBackwardPm), -50,   0));
        GPPB(Param(MP(PawnBackwardPe), -50,   0));
        GPPB(Param(MP(PawnDoubledPm),  -50,   0));
        GPPB(Param(MP(PawnDoubledPe),  -50,   0));
        GPPB(Param(MP(PawnIsoOpenPm),  -50,   0));
        GPPB(Param(MP(PawnIsoOpenPe),  -50,   0));
        GPPB(Param(MP(PawnIsolatedPm), -50,   0));
        GPPB(Param(MP(PawnIsolatedPe), -50,   0));
        GPPB(Param(MP(PawnConnectedPB),  1,  20));

        GPPB(Param(MP(PawnPassedGaurdedBm), 0, 50));
        GPPB(Param(MP(PawnPassedGaurdedBe), 0, 50));

        GPPB(Param(MP(PawnPassedStopOffset), 0, 50));

        for (int phase : { PhaseMg, PhaseEg }) {
            for (int type = Knight; type <= Queen; type++) {
                for (int i = 0; i < 2; i++) {
                    string name1 = format("PawnAttackB[%i][%i][%i]", phase, i, type);
                    string name2 = format("PawnAttackPushB[%i][%i][%i]", phase, i, type);

                    GPPB(Param(name1, &PawnAttackB[phase][i][type], 1, 200));
                    GPPB(Param(name2, &PawnAttackPushB[phase][i][type], 1, 200));
                }
            }
        }

        for (int rank = Rank2; rank < Rank7; rank++) {
            string name = format("PawnCandidateB[%i]", rank);

            int ub[8] = { 0, 25, 25, 50, 100, 200, 0, 0 };

            GPPB(Param(name, &PawnCandidateB[rank], 0, ub[rank]));
        }

        for (int phase : { PhaseMg, PhaseEg }) {
            for (int i = 0; i < 3; i++) {
                // Passed pawned bonus is always 0 in this case since an 
                // unstoppable passed pawn is assumed to be impossible when
                // the other side has at least one piece
                if (phase == PhaseMg && i == 1) continue;

                for (int rank = Rank1; rank <= Rank8; rank++) {
                    int ub;

                    // Rank indices 0 and 7 are reserved for king distance factors
                    if (rank == Rank1 || rank == Rank8)
                        ub = 25;
                    else
                        ub = (50 + 100 * phase) * rank;

                    string name = format("PawnPassedB[%i][%i][%i]", phase, i, rank);

                    GPPB(Param(name, &PawnPassedB[phase][i][rank], 0, ub));
                }
            }
        }

        for (int rank = Rank2; rank < Rank8; rank++) {
            string name = format("PawnConnectedB[%i]", rank);

            GPPB(Param(name, &PawnConnectedB[rank], 0, 10 * rank));
        }
    }

    if (TuneCurrent & TuneComplexity) {
        GPPB(Param(MP(ComplexityPawnsTotal),      0,  16));
        GPPB(Param(MP(ComplexityPawnsFlanked),    0, 150));
        GPPB(Param(MP(ComplexityPawnsEndgame),    0, 250));
        GPPB(Param(MP(ComplexityAdjustment),   -200, 200));
    }

    if (TuneCurrent & TuneScale) {
        GPPB(Param(MP(ScaleOcbBishopsOnly),      16, 128));
        GPPB(Param(MP(ScaleOcbOneKnight),        16, 128));
        GPPB(Param(MP(ScaleOcbOneRook),          16, 128));
        GPPB(Param(MP(ScaleLoneQueen),           64, 224));
        GPPB(Param(MP(ScaleLargePawnAdv),        64, 224));
        GPPB(Param(MP(ScalePNone),                8, 126));
        GPPB(Param(MP(ScalePOne),                 9, 127));
        GPPB(Param(MP(ScalePTwo),                10, 128));
    }

    if (TuneCurrent & TuneTempo)
        GPPB(Param(MP(TempoB), 1, 50));

    if (TuneCurrent & TuneImbalance) {
        for (int phase : { PhaseMg, PhaseEg }) {
            string name3 = format("RookKnightImbB[%i]",      phase);
            string name4 = format("RookBishopImbB[%i]",      phase);
            string name5 = format("RookTwoMinorsImbB[%i]",   phase);
            string name6 = format("QueenTwoRooksImbB[%i]",   phase);
            string name7 = format("QueenRookKnightImbB[%i]", phase);
            string name8 = format("QueenRookBishopImbB[%i]", phase);

            GPPB(Param(name3, &RookKnightImbB[phase],      -200, 200));
            GPPB(Param(name4, &RookBishopImbB[phase],      -200, 200));
            GPPB(Param(name5, &RookTwoMinorsImbB[phase],   -200, 200));
            GPPB(Param(name6, &QueenTwoRooksImbB[phase],   -200, 200));
            GPPB(Param(name7, &QueenRookKnightImbB[phase], -200, 200));
            GPPB(Param(name8, &QueenRookBishopImbB[phase], -200, 200));
        }
    }

#undef MP
#undef MPS
#undef GPPB

    if (tuner.debug) {
#if 0
        const double err = eval_error(g_pos, KOptimal);

        cout << err << endl;
        
        cout << "name,vmin,start,vmax,err0,err1,dx0,emx,emn,err2,dx1" << endl;
        for (Param param : g_params) {
            string name = param.name;

            int vmin = param.min;
            int vmax = param.max;
            
            *param.pvalue = vmin;
            double err0 = eval_error(g_pos, KOptimal);

            *param.pvalue = vmax;
            double err1 = eval_error(g_pos, KOptimal);

            *param.pvalue = param.start + 1;
            double err2 = eval_error(g_pos, KOptimal);

            *param.pvalue = param.start;
            
            double dx0 = abs((err1 - err0) / (vmax - vmin));
            double dx1 = abs(err2 - err);
                 
            double emx = max(abs(err - err0), abs(err - err1));
            double emn = min(abs(err - err0), abs(err - err1));

            cout << name << ','
                 << fixed << setprecision(10) 
                 << vmin << ',' 
                 << param.start << ',' 
                 << vmax << ',' 
                 << err0 << ',' 
                 << err1 << ',' 
                 << dx0  << ',' 
                 << emx  << ','
                 << emn  << ','
                 << err2 << ',' 
                 << dx1  << endl;
        }
#endif
    }

    if (!tuner.opt)
        return;

    cout << tuner.str() << endl;

    for (Param param : g_params)
        cout << param.str() << endl;

    nlopt::opt opt(tuner.local ? nlopt::LN_SBPLX : nlopt::GN_CRS2_LM, g_params.size());
    
    if (tuner.maxtime)
        opt.set_maxtime(60 * 60 * tuner.maxtime);
    
    g_params_orig = g_params;

    // Random shuffle of parameters
    shuffle(g_params.begin(), g_params.end(), mt);
    
    vector<double> x = tune_init(opt);

    try {
        double minf;

        int btime = time(nullptr);
        opt.optimize(x, minf);
        int etime = time(nullptr);

        cout << "opt complete" << endl;
        cout << (etime - btime) << ',' << minf << endl;

        for (size_t j = 0; j < g_params_orig.size(); j++)
            cout << g_params_orig[j].name << " = " << int(round(x[j])) << endl;

    }
    catch (exception& e) {
        cout << "nlopt failed: " << e.what() << endl;
    }
}

#endif
