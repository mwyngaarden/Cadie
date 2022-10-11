#include <array>
#include <iomanip>
#include "eval.h"
#include "gen.h"
#include "piece.h"
#include "see.h"
#include "util.h"

using namespace std;

const int SEEValue[PieceCount] = {
    ValuePiece[Pawn][PhaseMg],
    ValuePiece[Knight][PhaseMg],
    ValuePiece[Bishop][PhaseMg],
    ValuePiece[Rook][PhaseMg],
    ValuePiece[Queen][PhaseMg],
    ValuePiece[King][PhaseMg]
};

using AttList = Util::List<int, 16>;

struct CmbList { AttList alist[2]; };

static void see_hidden  (CmbList& clist, const Position& pos, int orig, int dest);
static int  see_recurse (CmbList& clist, const Position& pos, int side, int dest, int avalue);
static void see_attacks (AttList& alist, const Position& pos, int side, int dest);
static void see_add     (AttList& alist, const Position& pos, int sq);

int see_move(const Position& pos, Move m)
{
    CmbList clist;

    const int orig = m.orig();
    const int dest = m.dest();
    
    u8 piece = pos[orig];

    const int aside = is_black(piece);
    const int dside = flip_side(aside);

    if (m.is_promo())
        piece = to_piece256(aside, m.promo_piece());

    int avalue = SEEValue[P256ToP6[piece]];

    see_hidden(clist, pos, orig, dest);

    int dvalue = 0;

    const u8 cpiece = pos[dest];

    if (cpiece != PieceNone256)
        dvalue += SEEValue[P256ToP6[cpiece]];

    if (m.is_promo())
        dvalue += SEEValue[P256ToP6[piece]] - SEEValue[Pawn];
    else if (m.is_ep()) {
        dvalue += SEEValue[Pawn];

        see_hidden(clist, pos, ep_dual(dest), dest);
    }

    // defenders

    {
        AttList& alist = clist.alist[dside];

        see_attacks(alist, pos, dside, dest);

        if (alist.empty())
            return dvalue;
    }

    // attackers
   
    {
        AttList& alist = clist.alist[aside];

        see_attacks(alist, pos, aside, dest);

        if (alist.find(orig) != AttList::npos)
            alist.remove(orig);
    }

    dvalue -= see_recurse(clist, pos, dside, dest, avalue);

    return dvalue;
}

int see_recurse(CmbList& clist, const Position& pos, int side, int dest, int avalue)
{
    int orig;
    int dvalue;
    u8 piece;

    if (clist.alist[side].empty())
        return 0;

    orig = clist.alist[side].back();
    clist.alist[side].pop_back();

    if (orig == SquareNone) {
        assert(false);
        return 0;
    }

    see_hidden(clist, pos, orig, dest);

    dvalue = avalue;

    if (dvalue == ValueKing[PhaseMg])
        return dvalue;

    piece = pos[orig];
    avalue = SEEValue[P256ToP6[piece]];

    if (avalue == SEEValue[Pawn] && (sq88_rank(dest) == Rank1 || sq88_rank(dest) == Rank8)) {
        avalue  = SEEValue[Queen];
        dvalue += SEEValue[Queen] - SEEValue[Pawn];
    }

    dvalue -= see_recurse(clist, pos, flip_side(side), dest, avalue);

    return max(dvalue, 0);
}

void see_attacks(AttList& alist, const Position& pos, int side, int dest)
{
    assert(sq88_is_ok(dest));
    assert(side_is_ok(side));

    // knights

    for (auto orig : pos.plist(WN12 + side))
        if (pseudo_attack(orig, dest, KnightFlag256))
            see_add(alist, pos, orig);

    // sliders

    for (const auto& p : P12Flag) {
        for (const auto orig : pos.plist(p.first + side)) {
            if (pseudo_attack(orig, dest, p.second)) {
                const int incr = delta_incr(orig, dest);

                int sq = orig;

                do {
                    sq += incr;

                    if (sq == dest) {
                        see_add(alist, pos, orig);
                        break;
                    }
                } while (pos[sq] == PieceNone256);
            }
        }
    }

    // pawns

    const int pincr = pawn_incr(side);
    const u8 pawn = make_pawn(side);

    if (int orig = dest - pincr - 1; pos[orig] == pawn) see_add(alist, pos, orig);
    if (int orig = dest - pincr + 1; pos[orig] == pawn) see_add(alist, pos, orig);

    // king

    const int king = pos.king_sq(side);

    if (pseudo_attack(king, dest, KingFlag256)) see_add(alist, pos, king);
}

void see_hidden(CmbList& clist, const Position& pos, int orig, int dest)
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));

    const int incr = delta_incr(orig, dest);

    if (incr == 0)
        return;

    int sq = orig;

    u8 piece;

    do { sq -= incr; } while ((piece = pos[sq]) == PieceNone256);

    if (piece != PieceInvalid256 && is_slider(piece) && pseudo_attack(orig, dest, piece))
        see_add(clist.alist[is_black(piece)], pos, sq);
}

void see_add(AttList& alist, const Position& pos, int sq)
{
    assert(sq88_is_ok(sq));

    const u8 piece = pos[sq];

    const int ptype = P256ToP6[piece];

    int index;

    for (index = alist.size(); index > 0 && ptype > P256ToP6[pos[alist[index - 1]]]; index--)
        ;

    alist.insert(index, sq);
}

void see_validate()
{
    const int p = SEEValue[Pawn];
    const int n = SEEValue[Knight];
    const int b = SEEValue[Bishop];
    const int r = SEEValue[Rook];
    const int q = SEEValue[Queen];

    // shamelessly ripped from https://github.com/jdart1/arasan-chess/src/unit.cpp
    
    vector<SeeInfo> see_info {
        { "4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0 },
        { "4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0 },
        { "4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -", "g4f3", n - b },
        { "2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -", "d6e5", p },
        { "7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", 0 },
        { "6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -r },
        { "7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -r },
        { "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8q", b - p },
        { "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8n", n - p },
        { "7R/5P2/8/8/8/3K2r1/5p2/4k3 w - -", "f7f8q", q - p },
        
        { "7R/5P2/8/8/8/3K2r1/5p2/4k3 w - -", "f7f8b", b - p },
        { "7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f7f8r", -p },
        { "8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "h1f1", 0 },
        { "8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "h1f1", 0 },
        { "2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -", "c5c1", 2 * r - q },
        { "r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -", "f3e5", n - n + p },
        { "6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "f4e5", 0 },
        { "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "g5h6", 0 },
        { "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "g5h6", p },
        { "2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", r },
        
        { "2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", r },
        { "2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -", "c3c5", b },
        { "8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -", "g2c6", p - b },
        { "4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "e6e4", p - r },
        { "4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "h7e4", p }
    };

    int index = 0;
    int failed = 0;

    cout << dec << setfill(' ');

    for (auto si : see_info) {
        Position pos(si.fen);

        Move m = pos.note_move(si.move);

        int see = see_move(pos, m);
        
        bool pass = see == si.value;

        failed += !pass;

        cout << "i " << setw(3) << right << index
             << " p " << right << pass
             << " w " << setw(5) << right << si.value
             << " h " << setw(5) << right << see
             << " pos " << si.fen << endl;

        index++;
    }

    cout << "see failures " << failed << endl;
}
