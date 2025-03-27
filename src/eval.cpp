#include <cmath>
#include <cstdarg>
#include "attacks.h"
#include "bb.h"
#include "eval.h"
#include "gen.h"
#include "ht.h"
#include "search.h"

using namespace std;

// Globals

HashTable<64 * 1024 * 1024, EvalEntry> etable;

// Typedefs/Enums

enum { FileSemiOpen, FileOpen, FileClosed };

// Constants

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

// Structures/Classes

struct Attacks {
    u64 ks_zone;
    u64 ks_weak;

    int ks_attackers = 0;
    int ks_weight = 0;

    u64 ks_natts;
    u64 ks_batts;
    u64 ks_ratts;

    u64 lte[6] = { };
    u64 fat = 0;
    u64 all = 0;

    u64 queen_attacks = 0;

    u64 outposts = 0;

    int files[8];

    u64 mob_area[6] = { };
};

struct AttackInfo {
    u64 orig[64] = { };
    u64 passed = 0;

    Attacks attacks[2];

    Attacks& operator()(Side sd) { return attacks[sd]; }
    const Attacks& operator()(Side sd) const { return attacks[sd]; }
};

// Function protos

template <Side SD> static Value eval_attacks(const Position& pos, const AttackInfo& ai);
template <Side SD> static Value eval_king   (const Position& pos, const AttackInfo& ai);
template <Side SD> static Value eval_pieces (const Position& pos,       AttackInfo& ai);
template <Side SD> static Value eval_imbal  (const Position& pos);
template <Side SD> static Value eval_passed (const Position& pos, int orig, const AttackInfo& ai);

template <Side SD> static int eval_king     (const Position& pos);
template <Side SD> static int eval_shelter  (const Position& pos, int king);
template <Side SD> static int eval_storm    (const Position& pos, int king);

static int eval_scale    (const Position& pos, int score, const AttackInfo& ai);
static Value eval_compl  (const Position& pos, int score);
static Value eval_pattern(const Position& pos);

// Static variables

static u8 SqRelative32[2][64];

// Evaluation parameters

int RookBlockedPm                   =     -53;
int RookBlockedPe                   =     -29;
int NQAttBm                         =      35;
int NQAttBe                         =      25;
int BQAttBm                         =      61;
int BQAttBe                         =      71;
int RQAttBm                         =      66;
int RQAttBe                         =      23;
int KnightForkBm                    =      27;
int KnightForkBe                    =      39;
int KnightForkSafeB                 =       3;
int CenterControlBm                 =       3;
int CenterControlBe                 =       4;
int KingPawnlessFlankPm             =      37;
int KingPawnlessFlankPe             =     -48;
int NKPinByBPm                      =     -72;
int NKPinByBPe                      =    -133;
int NKPinByRPm                      =     -58;
int NKPinByRPe                      =     -51;
int NKPinByQPm                      =     -44;
int NKPinByQPe                      =     -24;
int BKPinByRPm                      =     -51;
int BKPinByRPe                      =     -64;
int BKPinByQPm                      =     -31;
int BKPinByQPe                      =     -31;
int RKPinByBPm                      =    -149;
int RKPinByBPe                      =    -332;
int RKPinByQPm                      =     -33;
int RKPinByQPe                      =     -80;
int QKPinByBPm                      =    -126;
int QKPinByBPe                      =    -346;
int QKPinByRPm                      =    -119;
int QKPinByRPe                      =    -291;
int NQPinPm                         =     -39;
int NQPinPe                         =    -118;
int BQPinPm                         =     -47;
int BQPinPe                         =    -115;
int RQPinPm                         =    -146;
int RQPinPe                         =    -187;
int MinorBehindPawnBm               =       7;
int MinorBehindPawnBe               =       6;
int KnightOutpostBm                 =      17;
int KnightOutpostBe                 =      18;
int KnightOutpostNearBm             =      12;
int KnightOutpostNearBe             =       4;
int KnightDistMulPm                 =     -14;
int KnightDistMulPe                 =      -5;
int BishopOutpostBm                 =      22;
int BishopOutpostBe                 =      15;
int BishopOutpostNearBm             =      11;
int BishopOutpostNearBe             =       2;
int RookClosedPm                    =       2;
int RookClosedPe                    =      39;
int RookOpenAdjKingBm               =      50;
int RookOpenAdjKingBe               =      46;
int RookOpenBm                      =      26;
int RookOpenBe                      =      52;
int RookOpenSameKingBm              =     116;
int RookOpenSameKingBe              =      22;
int RookSemiAdjKingBm               =      19;
int RookSemiAdjKingBe               =      43;
int RookSemiBm                      =       6;
int RookSemiBe                      =      74;
int RookSemiSameKingBm              =      58;
int RookSemiSameKingBe              =      21;
int RRSameFileBm                    =      14;
int RRSameFileBe                    =      -1;
int RQSameFileBm                    =       4;
int RQSameFileBe                    =      37;
int PawnBackOpenPm                  =     -15;
int PawnBackOpenPe                  =     -12;
int PawnBackwardPm                  =      -2;
int PawnBackwardPe                  =     -13;
int PawnDoubledPm                   =      -4;
int PawnDoubledPe                   =     -13;
int PawnIsoOpenPm                   =      -8;
int PawnIsoOpenPe                   =     -18;
int PawnIsolatedPm                  =       2;
int PawnIsolatedPe                  =     -17;
int PawnConnectedPB                 =      10;
int PawnPassedGaurdedBm             =      16;
int PawnPassedGaurdedBe             =      24;
int PawnPassedStopOffset            =      14;
int TempoB                          =      28;
int BishopPairBm                    =      19;
int BishopPairBe                    =      71;
int BishopPawnsPm                   =      -4;
int BishopPawnsPe                   =      -6;

int MobB[2][6][8] = {
    {
        {0,0,0,0,0,0,0,0},
        {-27,-7,1,8,15,22,0,0},
        {-4,3,9,16,20,19,51,0},
        {-13,-12,-9,-7,-4,-1,8,0},
        {-38,-34,-31,-27,-23,-18,-8,-17},
        {0,0,0,0,0,0,0,0}
    },
    {
        {0,0,0,0,0,0,0,0},
        {65,98,119,136,151,146,0,0},
        {69,93,109,126,144,150,124,0},
        {81,107,114,126,141,155,157,0},
        {253,283,303,317,332,346,353,359},
        {0,0,0,0,0,0,0,0}
    }
};

int PawnCandidateB[8] = {0,0,12,19,29,131,0,0};

int PawnConnectedB[8] = {0,0,2,6,16,28,32,0};

int PawnPassedB[2][3][8] = {
    {
        {3,0,0,0,26,56,163,-1},
        {0,0,0,0,0,0,0,0},
        {2,-3,-5,-9,12,49,147,0}
    },
    {
        {14,0,3,42,71,106,200,20},
        {-3,54,49,181,335,532,712,9},
        {14,2,13,66,122,232,400,17}
    }
};

int ScalePawn[4] = {51,59,65,68};

int KingShelterP[4][8] = {
    {0,0,1,-7,-11,-13,-38,-16},
    {0,0,-10,-35,-39,-31,-60,-30},
    {0,0,-18,-22,-25,-29,-47,-27},
    {0,0,-16,-20,-22,-27,-29,-21}
};

int KingStormP[4][8] = {
    {0,0,-8,3,0,0,0,0},
    {0,0,-18,-4,-7,0,0,0},
    {0,0,-29,-17,-4,0,0,0},
    {0,0,-20,-17,-8,0,0,0}
};

int KingSafetyW[6][8] = {
    {0,0,4,0,0,0,0,0},
    {0,10,15,14,9,0,0,0},
    {0,0,16,11,8,9,9,0},
    {0,17,13,14,12,9,8,7},
    {0,0,22,14,8,5,4,0},
    {0,0,0,0,0,0,0,0}
};

int PiecePawnOffset[2][6] = {
    {0,4,1,-7,-1,0},
    {0,2,-2,-2,-12,0}
};

int RookMinorImbB[2] = {73,60};

int RookTwoMinorsImbB[2] = {0,22};

int QueenTwoRooksImbB[2] = {156,-81};

int QueenRookMinorImbB[2] = {137,39};

int CheckDistantB[2][6] = {
    {0,69,9,40,18,0},
    {0,6,34,17,62,0}
};

int CheckContactB[2][6] = {
    {0,0,46,40,99,0},
    {0,0,22,26,-4,0}
};

int PawnAttB[2][2][6] = {
    {
        {0,64,70,62,59,0},
        {0,66,63,54,56,0}
    },
    {
        {0,46,59,59,88,0},
        {0,52,76,114,99,0}
    }
};

int PawnAttPushB[2][2][6] = {
    {
        {0,9,9,13,12,0},
        {0,16,16,13,14,0}
    },
    {
        {0,6,-7,16,-1,0},
        {0,7,12,34,16,0}
    }
};

int PieceAttHangingB[2][2][6] = {
    {
        {-2,25,18,28,11,0},
        {15,71,77,159,-17,0}
    },
    {
        {32,30,38,21,-12,0},
        {32,70,90,86,619,0}
    }
};

int KingAttB[2][6] = {
    {74,78,111,98,0,0},
    {58,33,34,17,0,0}
};

int PSQT[2][6][32] = {
    {
        {0,0,0,0,58,65,55,57,55,51,52,54,60,58,57,60,69,67,61,74,79,86,98,98,55,65,95,106,0,0,0,0},
        {179,221,223,228,226,227,237,239,231,244,244,248,248,256,262,253,264,254,274,259,242,252,267,257,243,228,269,258,169,198,182,218},
        {260,264,243,250,259,263,267,255,255,265,267,261,258,262,267,273,248,263,270,272,257,251,265,254,220,242,242,240,210,218,201,174},
        {277,286,287,291,269,277,279,280,264,277,266,272,268,277,270,273,279,288,289,295,284,304,295,308,293,293,304,301,296,299,267,273},
        {567,572,571,575,568,578,576,576,572,575,573,566,573,577,570,558,573,570,574,559,571,567,565,565,553,542,560,542,547,563,562,529},
        {3,0,-13,-55,24,23,-5,-15,9,9,9,-15,-14,-1,-9,-31,-12,8,-14,-44,7,50,15,-29,-20,67,48,8,31,95,124,53}
    },
    {
        {0,0,0,0,133,133,146,140,131,129,132,132,133,130,125,120,143,134,125,112,161,154,140,122,146,135,112,97,0,0,0,0},
        {346,334,347,354,343,354,346,358,342,358,361,378,362,365,382,387,360,370,378,389,347,357,374,375,342,364,355,371,293,340,367,365},
        {376,375,384,387,366,377,380,389,384,390,397,407,384,391,406,405,392,399,398,408,393,403,404,399,393,398,402,401,397,403,407,416},
        {627,628,627,622,620,625,624,623,632,635,640,636,649,657,661,653,660,669,665,663,668,666,668,661,677,681,676,676,677,678,684,679},
        {1180,1170,1165,1185,1187,1178,1182,1187,1194,1210,1214,1215,1221,1231,1236,1245,1229,1256,1251,1266,1222,1254,1269,1271,1256,1276,1272,1288,1258,1261,1262,1285},
        {-53,-24,-16,-17,-27,-16,-3,-2,-18,-8,5,16,-13,9,25,32,4,38,50,52,17,49,60,60,-9,30,33,31,-131,-51,-41,-25}
    }
};

AttackInfo eval_attack_info(const Position& pos)
{
    AttackInfo ai;

    u64 occ = pos.occ();

    for (Side sd : { White, Black }) {
        Attacks& attacks = ai.attacks[sd];

        int king = pos.king(sd);

        u64 spawns = pos.bb(sd, Pawn);
        u64 xpawns = pos.bb(!sd, Pawn);

        attacks.ks_zone  = bb::KingZone[king];
        attacks.ks_natts = KnightAttacks[king];
        attacks.ks_batts = bb::Leorik::Bishop(king, occ);
        attacks.ks_ratts = bb::Leorik::Rook(king, occ);

        attacks.all = bb::PawnAttacks(sd, spawns);
        attacks.lte[Pawn] = attacks.all;

        // Knight

        attacks.lte[Knight] = attacks.lte[Pawn];

        for (u64 bb = pos.bb(sd, Knight); bb; ) {
            int sq = bb::pop(bb);

            u64 att = KnightAttacks[sq];

            ai.orig[sq] = att;
            attacks.lte[Knight] |= att;
            attacks.fat |= att & attacks.all;
            attacks.all |= att;
        }

        // Bishop

        attacks.lte[Bishop] = attacks.lte[Knight];

        for (u64 bb = pos.bb(sd, Bishop); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Bishop(sq, occ);

            ai.orig[sq] = att;
            attacks.lte[Bishop] |= att;
            attacks.fat |= att & attacks.all;
            attacks.all |= att;
        }

        // Rook

        attacks.lte[Rook] = attacks.lte[Bishop];

        for (u64 bb = pos.bb(sd, Rook); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Rook(sq, occ);

            ai.orig[sq] = att;
            attacks.lte[Rook] |= att;
            attacks.fat |= att & attacks.all;
            attacks.all |= att;
        }

        // Queen

        attacks.lte[Queen] = attacks.lte[Rook];

        for (u64 bb = pos.bb(sd, Queen); bb; ) {
            int sq = bb::pop(bb);

            u64 att = bb::Leorik::Queen(sq, occ);

            ai.orig[sq] = att;
            attacks.lte[Queen] |= att;
            attacks.fat |= att & attacks.all;
            attacks.all |= att;

            attacks.queen_attacks |= att;
        }

        // King

        attacks.lte[King] = attacks.lte[Queen];

        {
            u64 att = KingAttacks[king];

            ai.orig[king] = att;
            attacks.lte[King] |= att;
            attacks.fat |= att & attacks.all;
            attacks.all |= att;
        }

        attacks.lte[Knight] |= attacks.lte[Bishop];

        for (int i = 0; i < 8; i++) {
            u64 file = bb::Files[i];

            attacks.files[i] = file & spawns ? FileClosed
                             : file & xpawns ? FileSemiOpen
                             : FileOpen;
        }

        attacks.outposts = bb::Outposts[sd] & attacks.lte[Pawn] & ~bb::PawnAttacksSpan(!sd, xpawns);
        attacks.ks_weak = attacks.lte[King] & ~attacks.lte[Queen];
    }

    for (Side sd : { White, Black }) {
        Side xd = !sd;

        u64 socc = pos.bb(sd);

        ai.attacks[sd].mob_area[Knight] = ~(socc | ai.attacks[xd].lte[Pawn]);
        ai.attacks[sd].mob_area[Bishop] = ~(socc | ai.attacks[xd].lte[Pawn]);
        ai.attacks[sd].mob_area[Rook]   = ~(socc | ai.attacks[xd].lte[Bishop]);
        ai.attacks[sd].mob_area[Queen]  = ~(socc | ai.attacks[xd].lte[Rook]);
    }

    return ai;
}

u8 sq_rel32(Side sd, int sq)
{
    int file = square::file(sq);
    int rank = square::rank(sq);

    file ^= 7 * (file >= 4);
    rank ^= 7 * sd;

    return 4 * rank + file;
}

void eval_init()
{
    for (Side sd : { White, Black })
        for (int i = 0; i < 64; i++)
            SqRelative32[sd][i] = sq_rel32(sd, i);
}

Value eval_psqt(Side sd, Piece pt, int sq)
{
    sq = SqRelative32[sd][sq];

    return { PSQT[PhaseMg][pt][sq], PSQT[PhaseEg][pt][sq] };
}

// Pawn evaluation

template <Side SD>
bool unstoppable_passer(const Position& pos, int orig)
{
    constexpr Side XD = !SD;
    constexpr int incr = square::incr(SD);

    int king = pos.king(XD);
    int rank = square::rank(orig, SD);

    if (pos.bb(SD) & bb::PawnSpan[SD][orig])
        return false;

    if (rank == Rank2) {
        orig += incr;
        rank++;
    }

    int prom = square::pawn_promo(SD, orig);
    int dist = bb::Dist[orig][prom];

    if (pos.side() == XD) dist++;

    return bb::Dist[king][prom] > dist;
}

template <Side SD>
bool king_passer(const Position& pos, int orig)
{
    int king = pos.king(SD);
    int file = square::file(orig);

    int prom = square::pawn_promo(SD, orig);

    if (   bb::Dist[king][prom] <= 1
        && bb::Dist[king][orig] <= 1
        && (square::file(king) != file || (file != FileA && file != FileH)))
        return true;

    return false;
}

template <Side SD>
Value eval_passed(const Position& pos, int orig, const AttackInfo& ai)
{
    constexpr Side XD = !SD;
    constexpr int incr = square::incr(SD);

    int sking   = pos.king(SD);
    int xking   = pos.king(XD);
    int dest    = orig + incr;
    int skdist  = bb::Dist[dest][sking];
    int xkdist  = bb::Dist[dest][xking];
    int rank    = square::rank(orig, SD);

    Value val;

    int idx = 0;

    if (!pos.pieces(XD) && (unstoppable_passer<SD>(pos, orig) || king_passer<SD>(pos, orig)))
        idx = 1;
    else if (pos.empty(dest) && !bb::test(ai(XD).all, dest))
        idx = 2;

    if (idx != 1) {
        u64 occ    = pos.occ();
        u64 majors = pos.majors(SD);
        u64 rspan  = bb::PawnSpan[XD][orig];

        for (u64 bb = rspan & pos.bb(SD, Rook); bb; ) {
            int sq = bb::pop(bb);

            u64 between = bb::Between[sq][orig] & occ;

            if ((between & ~majors) == 0)
                val += { PawnPassedGaurdedBm, PawnPassedGaurdedBe };
        }

        int psq   = square::pawn_promo(SD, orig);
        bool mdef = bb::test(ai(SD).all, psq);
        bool oatt = bb::test(ai(XD).all, psq);

        if (mdef && !oatt)
            val.eg += PawnPassedStopOffset;
        else if (!mdef && oatt)
            val.eg -= PawnPassedStopOffset;
    }

    int mkdist_mg = PawnPassedB[0][idx][0];
    int okdist_mg = PawnPassedB[0][idx][7];
    int mkdist_eg = PawnPassedB[1][idx][0];
    int okdist_eg = PawnPassedB[1][idx][7];

    int kdist_bm = okdist_mg * xkdist - mkdist_mg * skdist;
    int kdist_be = okdist_eg * xkdist - mkdist_eg * skdist;

    val += { PawnPassedB[0][idx][rank] + kdist_bm, PawnPassedB[1][idx][rank] + kdist_be };

    return val;
}

template <Side SD>
Value eval_pawns(const Position& pos, AttackInfo& ai)
{
    constexpr Side XD = !SD;
    constexpr int incr = square::incr(SD);

    Value val;

    int xking = pos.king(XD);

    u64 spawns = pos.pawns(SD);
    u64 xpawns = pos.pawns(XD);

    for (u64 bb = spawns; bb; ) {
        int orig = bb::pop(bb);

        val += eval_psqt(SD, Pawn, orig);

        // King Safety for XD

        if (PawnAttacks[SD][orig] & ai(XD).ks_zone) {
            ai(XD).ks_attackers++;
            ai(XD).ks_weight += KingSafetyW[Pawn][bb::Dist[xking][orig]];
        }

        u64 adjbb       = bb::FilesAdj[square::file(orig)];
        u64 span        = bb::PawnSpan[SD][orig];
        u64 span_adj    = bb::PawnSpanAdj[SD][orig];
        u64 rspan_adj   = bb::PawnSpanAdj[XD][orig];

        int rank = square::rank(orig, SD);

        bool backward   = false;
        bool isolated   = false;
        bool passed     = false;
        bool opposed    = false;

        bool open = !(span & (spawns | xpawns));

        if (open) {
            passed = !(xpawns & span_adj);

            if (passed) {
                ai.passed |= bb::bit(orig);

                val += eval_passed<SD>(pos, orig, ai);
            }
        }
        else
            opposed = xpawns & span;

        int phalanx = bb::count(spawns & adjbb & bb::Ranks64[orig]);
        int support = bb::count(spawns & PawnAttacks[XD][orig]);
        int levers  = bb::count(xpawns & PawnAttacks[SD][orig]);

        bool doubled = spawns & bb::PawnSpan[XD][orig];
        bool blocked = !pos.empty(orig + incr);

        if (support == 0 && phalanx == 0) {
            isolated = !(spawns & adjbb);

            if (!isolated) {
                u64 leversp = xpawns & PawnAttacks[SD][orig + incr];

                backward = !(spawns & rspan_adj) && (leversp || bb::test(xpawns, orig + incr));
            }
        }

        // Candidate

        if (open && !passed) {
            if (support >= levers) {
                int mp = bb::count(spawns & rspan_adj) + phalanx;
                int op = bb::count(xpawns & span_adj);

                if (mp >= op)
                    val.eg += PawnCandidateB[rank];
            }
        }

        // Bonuses

        if (phalanx || support) {
            int v = PawnConnectedB[rank] * (2 + !!phalanx - opposed) + PawnConnectedPB * support;

            val += { v, v * (rank - 2) / 4 };
        }

        for (u64 att = PawnAttacks[SD][orig] & pos.pieces(XD); att; ) {
            int sq = bb::pop(att);
            Piece pt = pos.square(sq) / 2;

            val += { PawnAttB[0][!!support][pt], PawnAttB[1][!!support][pt] };
        }

        if (!blocked) {
            for (u64 att = PawnAttacks[SD][orig + incr] & pos.pieces(XD); att; ) {
                int sq = bb::pop(att);
                Piece pt = pos.square(sq) / 2;

                val += { PawnAttPushB[0][!!phalanx][pt], PawnAttPushB[1][!!phalanx][pt] };
            }
        }

        // Penalties

        if (doubled)
            val += { PawnDoubledPm, PawnDoubledPe };

        if (isolated)
            val += open ? Value(PawnIsoOpenPm, PawnIsoOpenPe) : Value(PawnIsolatedPm, PawnIsolatedPe);
        else if (backward)
            val += open ? Value(PawnBackOpenPm, PawnBackOpenPe) : Value(PawnBackwardPm, PawnBackwardPe);
    }

    return val;
}

template <Side SD>
int eval_shelter(const Position& pos, int king)
{
    constexpr Side XD = !SD;

    int kfile = square::file(king);
    int krank = square::rank(king);

    // Adjust king position if on edge of board
   
    kfile = clamp(kfile, int(FileB), int(FileG));
    king = square::make(kfile, krank);

    int penalty = 0;

    u64 xocc   = pos.bb(XD);
    u64 spawns = pos.pawns(SD);

    for (int orig : { king-1, king, king+1 }) {
        int edge = square::dist_edge(orig);

        u64 bb = bb::PawnSpan[SD][orig] & (xocc | spawns);

        int rank = Rank8;

        if (bb & spawns) {
            int sq = SD == White ? bb::lsb(bb) : bb::msb(bb);

            if (!bb::test(xocc, sq))
                rank = square::rank(sq, SD);
        }

        penalty += KingShelterP[edge][rank];
    }

    return penalty;
}

template <Side SD>
int eval_storm(const Position& pos, int king)
{
    constexpr Side XD = !SD;

    int kfile = square::file(king);
    int krank = square::rank(king);

    // Adjust king position if on edge of board
   
    kfile = clamp(kfile, int(FileB), int(FileG));
    king = square::make(kfile, krank);

    int penalty = 0;

    u64 xpawns = pos.pawns(XD);
   
    for (int orig : { king-1, king, king+1 }) {
        int edge = square::dist_edge(orig);

        u64 bb = bb::PawnSpan[SD][orig] & xpawns;

        int rank = Rank1;

        if (bb) {
            int sq = SD == White ? bb::lsb(bb) : bb::msb(bb);

            rank = square::rank(sq, SD);
        }

        penalty += KingStormP[edge][rank];
    }
    
    return penalty;
}

template <Side SD>
int eval_king(const Position& pos)
{
    int king = pos.king(SD);

    int safety = eval_shelter<SD>(pos, king) + eval_storm<SD>(pos, king);

    if (pos.can_castle_k(SD)) {
        king = SD == White ? square::G1 : square::G8;

        int score = eval_shelter<SD>(pos, king) + eval_storm<SD>(pos, king);

        safety = max(safety, score);
    }
    
    if (pos.can_castle_q(SD)) {
        king = SD == White ? square::C1 : square::C8;

        int score = eval_shelter<SD>(pos, king) + eval_storm<SD>(pos, king);

        safety = max(safety, score);
    }

    return safety;
}

Value eval_compl(const Position& pos, int score)
{
    u64 pawns = pos.pawns();

    int wking = pos.king(White);
    int bking = pos.king(Black);

    int outflanking = abs(square::file(wking) - square::file(bking)) + square::rank(wking) - square::rank(bking);
    bool flanked = (pawns & bb::QueenSide) && (pawns & bb::KingSide);
    bool drawish = outflanking < 0 && !flanked;

    int c =   6 * bb::count(pawns)
          +  10 * flanked
          +   2 * outflanking
          + 135 * !pos.pieces()
          -  63 * drawish
          -  66;

    int v = ((score > 0) - (score < 0)) * max(c, -abs(score));

    return { 0, v };
}

int eval_scale(const Position& pos, int score, const AttackInfo& ai)
{
    u64 white   = pos.bb(White);
    u64 black   = pos.bb(Black);
    u64 pawns   = pos.bb(Pawn);
    u64 knights = pos.bb(Knight);
    u64 bishops = pos.bb(Bishop);
    u64 rooks   = pos.bb(Rook);
    u64 queens  = pos.bb(Queen);
    u64 minors  = knights | bishops;
    u64 pieces  = knights | bishops | rooks; // no queens

    bool ocb = pos.ocb();

    Side sd = score >= 0 ? White : Black;
    Side xd = !sd;

    u64 hi = sd == White ? white : black;
    u64 lo = sd == White ? black : white;

    int hpawns  = bb::count(hi & pawns);
    int hphase  = pos.phase(sd);
    int hpassed = bb::count(hi & ai.passed);
    int lpawns  = bb::count(lo & pawns);
    int lphase  = pos.phase(xd);

    if (hpawns == 0) {
        if (hphase <= 1)
            return 0;

        if (hphase == 2 && bb::multi(hi & knights) && lpawns == 0)
            return 0;

        if (hphase - lphase <= 1)
            return 0;
    }

    else if (hpawns == 1) {
        int sq = bb::lsb(hi & pawns);
        bool blocked = bb::test(bb::PawnSpan[sd][sq], pos.king(xd));

        if (hphase <= 1 && (lo & minors))
            return 28;

        if (hphase == 2 && bb::multi(hi & knights) && lpawns == 0 && (lo & minors))
            return 24;

        if (hphase == lphase && blocked)
            return 8;

        if (hphase == lphase && (lo & minors))
            return 15;
    }

    // Opposite colored bishops

    if (ocb) {
        bool two_bishops = bb::count(bishops) == 2;

        if (two_bishops && !queens) {
            if (!(knights | rooks))
                return 6 + 14 * hpassed;

            if (!rooks && bb::single(white & knights) && bb::single(black & knights))
                return 52;

            if (!knights && bb::single(white & rooks) && bb::single(black & rooks))
                return 57;
        }

        return 54 + 3 * bb::count(hi & pieces);
    }

    if (bb::single(queens))
        return 40 + 3 * bb::count((white & queens ? black : white) & minors);

    return ScalePawn[min(hpawns, 3)];
}

// King evaluation

template <Side SD>
Value eval_king(const Position& pos, const AttackInfo& ai)
{
    constexpr Side XD = !SD;

    const int ks_factors[5] = { 0, 10, 12, 15, 16 };

    Value val;

    if (pos.phase(XD) >= 5)
        val.mg += eval_king<SD>(pos);

    int king = pos.king(SD);
    int file = square::file(king);

    u64 flank = file <= FileD ? bb::QueenSide : bb::KingSide;
    u64 camp = SD == White ? bb::RanksForward[Black][Rank6] : bb::RanksForward[White][Rank3];
    int def = bb::count(ai(SD).all & flank & camp);

    int attackers = min(ai(SD).ks_attackers, 4);
    int weight = ai(SD).ks_weight;

    u64 weak =   ai(XD).all
             &  ~ai(SD).fat
             & (~ai(SD).all | KingAttacks[king] | ai(SD).queen_attacks);

    int ks = weight * ks_factors[attackers]
           - 371 * !pos.count(WQ12 + XD)
           +  66 * bb::count(ai(SD).ks_zone & weak)
           -  57 * bool(bb::KnightAttacks(pos.bb(SD, Knight)) & KingAttacks[king])
           -  30 * def
           - 105 * val.mg / 32
           + 533;

    val.mg -= ks * ks / 4096;

    // PSQT

    val += eval_psqt(SD, King, king);

    // King on pawnless flank

    if ((pos.bb(SD, Pawn) & flank) == 0)
        val += { KingPawnlessFlankPm, KingPawnlessFlankPe };

    return val;
}

// Piece evaluation

template <Side SD>
Value eval_pieces(const Position& pos, AttackInfo& ai)
{
    constexpr Side XD = !SD;
    constexpr int incr = square::incr(SD);

    Value val;

    int sking = pos.king(SD);
    int xking = pos.king(XD);

    int xkfile = square::file(xking);

    u64 spawns  = pos.bb(SD, Pawn);
    u64 srooks  = pos.bb(SD, Rook);
    u64 squeens = pos.bb(SD, Queen);
    u64 xqueens = pos.bb(XD, Queen);

    int npawns = pos.count(WP12 + SD);

    u64 xkzone = ai(XD).ks_zone;
    u64 xatt = ai(XD).all;

    u64 safe_checks[6] = {
        0,
        ai(XD).ks_natts & ~xatt,
        ai(XD).ks_batts & ~xatt,
        ai(XD).ks_ratts & ~xatt,
        (ai(XD).ks_batts | ai(XD).ks_ratts) & ~xatt,
        0
    };

    // Center control

    u64 control = bb::Center16 & ai(SD).fat & ~ai(XD).fat;

    val += Value(CenterControlBm, CenterControlBe) * bb::count(control);

    // Piece loop

    for (u64 bb = pos.pieces(SD); bb; ) {
        int orig = bb::pop(bb);
        Piece pt = pos.square(orig) / 2;

        // PSQT

        val += eval_psqt(SD, pt, orig);

        u64 mask = ai.orig[orig];

        // Mobility

        u64 targets = ai(SD).mob_area[pt];
        u64 atts = mask & targets;
        int mob = bb::count(atts);

        val += { MobB[0][pt][LOG[mob]], MobB[1][pt][LOG[mob]] };

        // King Safety for XD

        if (atts & xkzone) {
            ai(XD).ks_attackers++;
            ai(XD).ks_weight += KingSafetyW[pt][bb::Dist[xking][orig]];
        }

        // Checks

        if (mask & safe_checks[pt])
            val += { CheckDistantB[0][pt], CheckDistantB[1][pt] };


        int file = square::file(orig);
        int rank = square::rank(orig, SD);

        val += Value(PiecePawnOffset[0][pt], PiecePawnOffset[1][pt]) * (npawns - 4);

        if (pt == Knight) {
            if (bb::test(ai(SD).outposts, orig))
                val += { KnightOutpostBm, KnightOutpostBe };

            else if (ai(SD).outposts & mask)
                val += { KnightOutpostNearBm, KnightOutpostNearBe };

            if (rank <= Rank6 && bb::test(spawns, orig + incr))
                val += { MinorBehindPawnBm, MinorBehindPawnBe };

            int penalty = bb::Dist[orig][sking] * bb::Dist[orig][xking];

            val += { KnightDistMulPm * penalty / 16, KnightDistMulPe * penalty / 16 };

            if (mask & xqueens)
                val += { NQAttBm, NQAttBe };

            int safe = !bb::test(ai(XD).all, orig) ? KnightForkSafeB : 1;

            if (bb::multi(mask & pos.pieces(XD)))
                val += Value(KnightForkBm, KnightForkBe) * safe;
        }

        else if (pt == Bishop) {
            if (bb::test(ai(SD).outposts, orig))
                val += { BishopOutpostBm, BishopOutpostBe };

            else if (ai(SD).outposts & mask)
                val += { BishopOutpostNearBm, BishopOutpostNearBe };

            if (rank <= Rank6 && bb::test(spawns, orig + incr))
                val += { MinorBehindPawnBm, MinorBehindPawnBe };

            u64 color = square::color(orig) ? bb::Dark : bb::Light;

            val += Value(BishopPawnsPm, BishopPawnsPe) * bb::count(spawns & color);

            if (mask & xqueens)
                val += { BQAttBm, BQAttBe };

            if (mask & ai(SD).fat & (ai(XD).ks_weak & BishopAttacks[xking]))
                val += { CheckContactB[0][Bishop], CheckContactB[1][Bishop] };
        }

        else if (pt == Rook) {
            int fkind = ai(SD).files[file];
            int fdiff = abs(file - xkfile);

            if (fkind == FileClosed)
                val += { RookClosedPm, RookClosedPe };
            else {
                bool semiopen = fkind == FileSemiOpen;

                if (fdiff == 0)
                    val += semiopen ? Value(RookSemiSameKingBm, RookSemiSameKingBe)
                                    : Value(RookOpenSameKingBm, RookOpenSameKingBe);
                else if (fdiff == 1)
                    val += semiopen ? Value(RookSemiAdjKingBm, RookSemiAdjKingBe)
                                    : Value(RookOpenAdjKingBm, RookOpenAdjKingBe);
                else
                    val += semiopen ? Value(RookSemiBm, RookSemiBe)
                                    : Value(RookOpenBm, RookOpenBe);
            }

            if (mask & xqueens)
                val += { RQAttBm, RQAttBe };

            u64 fmask = bb::Files64[orig] & mask;

            if (fmask & srooks)  val += { RRSameFileBm, RRSameFileBe };
            if (fmask & squeens) val += { RQSameFileBm, RQSameFileBe };

            if (mask & ai(SD).fat & (ai(XD).ks_weak & RookAttacks[xking]))
                val += { CheckContactB[0][Rook], CheckContactB[1][Rook] };
        }

        else { // Queen
            if (mask & ai(SD).fat & ai(XD).ks_weak)
                val += { CheckContactB[0][Queen], CheckContactB[1][Queen] };
        }
    }

    return val;
}

template <Side SD>
Value eval_imbal(const Position& pos)
{
    Value val;

    auto [sn, sb, sr, sq, xn, xb, xr, xq] = pos.counts<SD>();

    int rdiff = sr - xr;
    int qdiff = sq - xq;
    int mdiff = (sn + sb) - (xn + xb);

    // Up rook

    if (rdiff == 1 && qdiff == 0) {
        if (mdiff == -1)
            val += { RookMinorImbB[0], RookMinorImbB[1] };

        else if (mdiff == -2)
            val += { RookTwoMinorsImbB[0], RookTwoMinorsImbB[1] };
    }

    // Up queen

    else if (qdiff == 1) {
        if (rdiff == -1 && mdiff == -1)
            val += { QueenRookMinorImbB[0], QueenRookMinorImbB[1] };

        else if (rdiff == -2 && mdiff == 0)
            val += { QueenTwoRooksImbB[0], QueenTwoRooksImbB[1] };
    }

    return val;
}

Value eval_pattern(const Position& pos)
{
    using namespace square;

    Value val;

    u64 br = pos.bb(Black, Rook);
    u64 bk = pos.bb(Black, King);
    u64 wr = pos.bb(White, Rook);
    u64 wk = pos.bb(White, King);

    // White

    constexpr u64 wqs_rmask = bb::bit(A1, A2, B1);
    constexpr u64 wqs_kmask = bb::bit(B1, C1);
    constexpr u64 wks_rmask = bb::bit(H1, H2, G1);
    constexpr u64 wks_kmask = bb::bit(F1, G1);

    if ((wr & wqs_rmask) && (wqs_kmask & wk)) val += { RookBlockedPm, RookBlockedPe };
    if ((wr & wks_rmask) && (wks_kmask & wk)) val += { RookBlockedPm, RookBlockedPe };

    // Black

    constexpr u64 bqs_rmask = bb::bit(A7, A8, B8);
    constexpr u64 bqs_kmask = bb::bit(B8, C8);
    constexpr u64 bks_rmask = bb::bit(H7, H8, G8);
    constexpr u64 bks_kmask = bb::bit(F8, G8);

    if ((br & bqs_rmask) && (bqs_kmask & bk)) val -= { RookBlockedPm, RookBlockedPe };
    if ((br & bks_rmask) && (bks_kmask & bk)) val -= { RookBlockedPm, RookBlockedPe };

    return val;
}

template <Side SD>
Value eval_abs_pins(const Position& pos)
{
    constexpr Side XD = !SD;

    Value val;

    u64 occ      = pos.occ();
    u64 spawns   = pos.pawns(SD);
    u64 xbishops = pos.bb(XD, Bishop, Queen);
    u64 xrooks   = pos.bb(XD, Rook, Queen);

    u64 pinners = pos.pinners();

    int king = pos.king(SD);

    for (u64 bb = BishopAttacks[king] & xbishops & pinners; bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[king][sq] & occ;

        if (between & spawns) continue;

        Piece pinned = pos.square(bb::lsb(between)) / 2;
        Piece pinner = pos.square(sq) / 2;

        if (pinned == Knight) {
            if (pinner == Bishop)
                val += { NKPinByBPm, NKPinByBPe };

            else if (pinner == Queen)
                val += { NKPinByQPm, NKPinByQPe };
        }

        else if (pinned == Rook) {
            if (pinner == Bishop)
                val += { RKPinByBPm, RKPinByBPe };

            else if (pinner == Queen)
                val += { RKPinByQPm, RKPinByQPe };
        }

        else if (pinned == Queen) {
            if (pinner == Bishop)
                val += { QKPinByBPm, QKPinByBPe };
        }
    }
    
    for (u64 bb = RookAttacks[king] & xrooks & pinners; bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[king][sq] & occ;

        if (between & spawns) continue;

        Piece pinned = pos.square(bb::lsb(between)) / 2;
        Piece pinner = pos.square(sq) / 2;

        if (pinned == Knight) {
            if (pinner == Rook)
                val += { NKPinByRPm, NKPinByRPe };

            else if (pinner == Queen)
                val += { NKPinByQPm, NKPinByQPe };
        }

        else if (pinned == Bishop) {
            if (pinner == Rook)
                val += { BKPinByRPm, BKPinByRPe };

            else if (pinner == Queen)
                val += { BKPinByQPm, BKPinByQPe };
        }

        else if (pinned == Queen) {
            if (pinner == Rook)
                val += { QKPinByRPm, QKPinByRPe };
        }
    }

    return val;
}

template <Side SD>
Value eval_rel_pins(const Position& pos)
{
    constexpr Side XD = !SD;

    Value val;

    u64 occ     = pos.occ();
    u64 pieces  = pos.bb(SD, Knight, Bishop, Rook);
    u64 queens  = pos.bb(SD, Queen);
    u64 bishops = pos.bb(XD, Bishop);
    u64 rooks   = pos.bb(XD, Rook);

    for (u64 bb = queens; bb; ) {
        int qsq = bb::pop(bb);

        u64 batt = BishopAttacks[qsq] & bishops;

        while (batt) {
            int bsq = bb::pop(batt);

            u64 between = bb::Between[qsq][bsq] & occ;

            if ((between & pieces) == 0 || !bb::single(between)) continue;

            Piece pinned = pos.square(bb::lsb(between)) / 2;

            if (pinned == Knight)
                val += { NQPinPm, NQPinPe };

            else if (pinned == Rook)
                val += { RQPinPm, RQPinPe };
        }

        u64 ratt = RookAttacks[qsq] & rooks;

        while (ratt) {
            int rsq = bb::pop(ratt);

            u64 between = bb::Between[qsq][rsq] & occ;

            if ((between & pieces) == 0 || !bb::single(between)) continue;

            Piece pinned = pos.square(bb::lsb(between)) / 2;

            if (pinned == Knight)
                val += { NQPinPm, NQPinPe };

            else if (pinned == Bishop)
                val += { BQPinPm, BQPinPe };
        }
    }

    return val;
}

template <Side SD>
Value eval_attacks(const Position& pos, const AttackInfo& ai)
{
    constexpr Side XD = !SD;

    Value val;

    int king = pos.king(SD);

    u64 katt = KingAttacks[king];
    u64 xocc = pos.bb(XD);
    u64 xatt = ai(XD).all;
    u64 weak = xocc & ~xatt;

    for (u64 bb = weak & katt; bb; ) {
        int sq = bb::pop(bb);
        Piece pt = pos.square(sq) / 2;

        val += { KingAttB[0][pt], KingAttB[1][pt] };
    }

    for (u64 bb = weak & ~katt & ai(SD).lte[Queen]; bb; ) {
        int sq = bb::pop(bb);
        Piece pt = pos.square(sq) / 2;
        bool fat = bb::test(ai(SD).fat, sq);

        val += { PieceAttHangingB[0][fat][pt], PieceAttHangingB[1][fat][pt] };
    }

    return val;
}

template <Side SD>
Value eval_side(const Position& pos, AttackInfo& ai)
{
    constexpr Side XD = !SD;

    Value val;

    val += eval_attacks<SD>(pos, ai);
    val += eval_pawns<SD>(pos, ai);

    if (pos.pinned(SD) & pos.pieces())
        val += eval_abs_pins<SD>(pos);

    if (pos.bb(SD, Queen) && pos.bb(SD, Knight, Bishop, Rook) && pos.bb(XD, Bishop, Rook))
        val += eval_rel_pins<SD>(pos);

    val += eval_imbal<SD>(pos);
    val += eval_pieces<SD>(pos, ai);

    return val;
}

int eval(const Position& pos)
{
    EvalEntry eentry;

    if (etable.get(pos.key(), eentry))
        return eentry.score + TempoB;

    AttackInfo ai = eval_attack_info(pos);

    Value val;

    // Bishop pair

    int wbp = pos.bishop_pair(White);
    int bbp = pos.bishop_pair(Black);

    val += (wbp - bbp) * Value(BishopPairBm, BishopPairBe);

    // Side

    val += eval_side<White>(pos, ai);
    val -= eval_side<Black>(pos, ai);

    // Pattern

    val += eval_pattern(pos);

    // King safety

    val += eval_king<White>(pos, ai);
    val -= eval_king<Black>(pos, ai);

    // Complexity

    val += eval_compl(pos, val.eg);

    int score = val.lerp(pos.phase(), eval_scale(pos, val.eg, ai));

    if (pos.side() == Black) score = -score;

    eentry.score = score;

    etable.set(pos.key(), eentry);
    
    return score + TempoB;
}
