#include <iomanip>
#include <iostream>
#include "attacks.h"
#include "bb.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "square.h"
using namespace std;


static size_t gen_pseudos  (MoveList& moves, const Position& pos, GenMode mode);
static size_t gen_evasions (MoveList& moves, const Position& pos);

static void   gen_pawn     (MoveList& moves, const Position& pos, u64 targets, bool tactical);
static void   gen_promos   (MoveList& moves, const Position& pos, u64 targets, bool tactical);
static void   gen_piece    (MoveList& moves, const Position& pos, u64 targets);

void gen_pawn(MoveList& moves, const Position& pos, u64 targets, bool tactical)
{
    Side sd = pos.side();
    Side xd = !pos.side();

    int incr = square::incr(sd);

    u64 occ   = pos.occ();
    u64 xocc  = pos.bb(xd);
    u64 pawns = pos.bb(sd, Pawn) & (sd == White ? ~bb::Rank7 : ~bb::Rank2);

    // Captures

    for (u64 bb = pawns; bb; ) {
        int psq = bb::pop(bb);

        u64 patt = PawnAttacks[sd][psq] & xocc & targets;

        while (patt) {
            int csq = bb::pop(patt);

            moves.add(Move(psq, csq, pos.square(csq)));
        }
    }

    // En passant

    if (int ep_sq = pos.ep_sq(); ep_sq != square::None) {
        int epdual = square::ep_dual(ep_sq);

        int file = square::file(epdual);

        if (file != FileA && bb::test(pawns, epdual - 1)) {
            Move m(epdual - 1, ep_sq, WP12 + xd);
            moves.add(m | Move::EPFlag);
        }

        if (file != FileH && bb::test(pawns, epdual + 1)) {
            Move m(epdual + 1, ep_sq, WP12 + xd);
            moves.add(m | Move::EPFlag);
        }
    }

    if (!tactical) {

        // Single pushes

        for (u64 bb = bb::PawnSingles(sd, pawns, ~occ) & targets; bb; ) {
            int psq = bb::pop(bb);

            moves.add(Move(psq - incr, psq) | Move::SingleFlag);
        }

        // Double pushes

        for (u64 bb = bb::PawnDoubles(sd, pawns, ~occ) & targets; bb; ) {
            int psq = bb::pop(bb);

            moves.add(Move(psq - 2 * incr, psq) | Move::DoubleFlag);
        }
    }
}

void gen_promos(MoveList& moves, const Position& pos, u64 targets, bool tactical)
{
    Side sd = pos.side();
    Side xd = !pos.side();

    int incr = square::incr(sd);

    u64 occ = pos.occ();
    u64 xocc = pos.bb(xd);

    u64 pawns = pos.bb(sd, Pawn) & (sd == White ? bb::Rank7 : bb::Rank2);

    // Captures

    for (u64 bb = pawns; bb; ) {
        int psq = bb::pop(bb);

        u64 patt = PawnAttacks[sd][psq] & xocc & targets;

        while (patt) {
            int csq = bb::pop(patt);

            Move m(psq, csq, pos.square(csq));

            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }
    }

    // Single pushes

    for (u64 bb = bb::PawnSingles(sd, pawns, ~occ) & targets; bb; ) {
        int psq = bb::pop(bb);

        Move m(psq - incr, psq);

        moves.add(m | Move::PromoQFlag);

        if (!tactical) {
            moves.add(m | Move::PromoRFlag);
            moves.add(m | Move::PromoBFlag);
            moves.add(m | Move::PromoNFlag);
        }
    }
}

void gen_piece(MoveList& moves, const Position& pos, u64 targets)
{
    Side sd = pos.side();

    u64 occ     = pos.occ();
    u64 knights = pos.bb(sd, Knight);
    u64 bishops = pos.bb(sd, Bishop);
    u64 rooks   = pos.bb(sd, Rook);
    u64 queens  = pos.bb(sd, Queen);

    // Knight

    for (u64 bb = knights; bb; ) {
        int psq = bb::pop(bb);

        u64 att = KnightAttacks[psq] & targets;

        while (att) {
            int csq = bb::pop(att);

            moves.add(Move(psq, csq, pos.square(csq)));
        }
    }

    // Bishop and Queen

    for (u64 bb = bishops | queens; bb; ) {
        int psq = bb::pop(bb);

        u64 att = bb::Leorik::Bishop(psq, occ) & targets;

        while (att) {
            int csq = bb::pop(att);

            moves.add(Move(psq, csq, pos.square(csq)));
        }
    }

    // Rook and Queen

    for (u64 bb = rooks | queens; bb; ) {
        int psq = bb::pop(bb);

        u64 att = bb::Leorik::Rook(psq, occ) & targets;

        while (att) {
            int csq = bb::pop(att);

            moves.add(Move(psq, csq, pos.square(csq)));
        }
    }
}

size_t gen_pseudos(MoveList& moves, const Position& pos, GenMode mode)
{
    bool tactical = mode == GenMode::Tactical;

    Side sd = pos.side();
    Side xd = !pos.side();

    int king = pos.king(sd);

    u64 occ = pos.occ();

    u64 targets = tactical ? pos.bb(xd) : ~pos.bb(sd);

    // Pawn

    gen_pawn(moves, pos, bb::Full, tactical);
    gen_promos(moves, pos, bb::Full, tactical);

    // Knight/Bishop/Rook/Queen

    gen_piece(moves, pos, targets);

    // King

    for (u64 bb = KingAttacks[king] & targets; bb; ) {
        int csq = bb::pop(bb);

        moves.add(Move(king, csq, pos.square(csq)));
    }

    // Castling

    if (!tactical) {
        if (pos.can_castle_k()) {
            u64 between = bb::Between[king][king + 3];

            if ((between & occ) == 0)
                moves.add(Move(king, king + 2) | Move::CastleFlag);
        }

        if (pos.can_castle_q()) {
            u64 between = bb::Between[king][king - 4];

            if ((between & occ) == 0)
                moves.add(Move(king, king - 2) | Move::CastleFlag);
        }
    }

    if (mode == GenMode::Legal) {
        for (size_t i = 0; i < moves.size(); i++) {
            Move m = moves[i];

            int orig = m.orig();

            bool validate = bb::test(pos.pinned(), orig) || orig == king || m.is_ep();

            if (validate && !pos.move_is_legal(m))
                moves.remove_at(i--);
        }
    }

    return moves.size();
}

size_t gen_evasions(MoveList& moves, const Position& pos)
{
    Side sd = pos.side();

    Position tmp = pos;

    int king     = pos.king();
    int checker1 = pos.checker1();

    u64 socc    = pos.bb(sd);
    u64 sliders = pos.sliders();
    u64 unsafe  = 0;

    if (bb::test(sliders, checker1))
        unsafe |= bb::Line[king][checker1] ^ bb::bit(checker1);

    // Double check

    if (!bb::single(pos.checkers())) {
        int checker2 = pos.checker2();

        if (bb::test(sliders, checker2))
            unsafe |= bb::Line[king][checker2] ^ bb::bit(checker2);

        u64 targets = ~(socc | unsafe);

        for (u64 bb = KingAttacks[king] & targets; bb; ) {
            int sq = bb::pop(bb);

            Move m(king, sq, pos.square(sq));

            if (tmp.move_is_sane(m))
                moves.add(m);
        }

        return moves.size();
    }

    // King

    for (u64 bb = KingAttacks[king] & ~(socc | unsafe); bb; ) {
        int sq = bb::pop(bb);

        Move m(king, sq, pos.square(sq));

        if (tmp.move_is_sane(m))
            moves.add(m);
    }

    u64 between = bb::Between[king][checker1];
    u64 targets = bb::bit(checker1) | between;

    // Pawn

    gen_pawn(moves, pos, targets, false);
    gen_promos(moves, pos, targets, false);

    // Knight/Bishop/Rook/Queen

    gen_piece(moves, pos, targets);

    // Validate

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig();

        bool validate = bb::test(pos.pinned(), orig) || m.is_ep();

        if (validate && !tmp.move_is_sane(m))
            moves.remove_at(i--);
    }

    return moves.size();
}

// Only direct checks are considered. Eventually add discovered checks?
size_t add_checks(MoveList& moves, const Position& pos)
{
    Side sd = pos.side();
    Side xd = !pos.side();

    int incr = square::incr(sd);
    int king = pos.king(xd);

    u64 occ    = pos.occ();
    u64 pawns  = pos.bb(sd, Pawn);

    // Only Knights/Bishops on same color as King can give check
    u64 color  = square::color(king) ? bb::Dark : bb::Light;

    // Pawns

    u64 targets = PawnAttacks[xd][king];

    for (u64 bb = bb::PawnSingles(sd, pawns, ~occ) & targets; bb; ) {
        int psq = bb::pop(bb);

        moves.add(Move(psq - incr, psq) | Move::SingleFlag);
    }

    for (u64 bb = bb::PawnDoubles(sd, pawns, ~occ) & targets; bb; ) {
        int psq = bb::pop(bb);

        moves.add(Move(psq - 2 * incr, psq) | Move::DoubleFlag);
    }

    // Knights

    targets = KnightAttacks[king] & ~occ;

    for (u64 bb = pos.bb(sd, Knight) & color; bb; ) {
        int psq = bb::pop(bb);

        u64 att = KnightAttacks[psq] & targets;

        while (att) {
            int csq = bb::pop(att);

            moves.add(Move(psq, csq));
        }
    }

    u64 queens   = pos.bb(sd, Queen);
    u64 btargets = bb::Leorik::Bishop(king, occ) & ~occ;
    u64 rtargets = bb::Leorik::Rook(king, occ) & ~occ;

    // Bishop and Queen

    for (u64 bb = (pos.bb(sd, Bishop) & color) | queens; bb; ) {
        int psq = bb::pop(bb);

        u64 att = BishopAttacks[psq] & btargets;

        while (att) {
            int csq = bb::pop(att);

            if ((bb::Between[psq][csq] & occ) == 0)
                moves.add(Move(psq, csq));
        }
    }

    // Rook and Queen

    for (u64 bb = pos.bb(sd, Rook) | queens; bb; ) {
        int psq = bb::pop(bb);

        u64 att = RookAttacks[psq] & rtargets;

        while (att) {
            int csq = bb::pop(att);

            if ((bb::Between[psq][csq] & occ) == 0)
                moves.add(Move(psq, csq));
        }
    }

    return moves.size();
}

size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode)
{
    size_t count = pos.checkers()
                 ? gen_evasions(moves, pos)
                 : gen_pseudos(moves, pos, mode);

    return count;
}

GenState gen_state(const Position& pos)
{
    MoveList moves;

    size_t count = pos.checkers()
                 ? gen_evasions(moves, pos)
                 : gen_pseudos(moves, pos, GenMode::Legal);

    if (count)
        return GenState::Normal;
    else if (pos.checkers())
        return GenState::Checkmate;
    else
        return GenState::Stalemate;
}

int delta_incr(int orig, int dest)
{
    if (orig == dest) return 0;

    if (square::rank_eq(orig, dest)) return orig < dest ? 1 : -1;
    if (square::file_eq(orig, dest)) return orig < dest ? 8 : -8;
    if (square::diag_eq(orig, dest)) return orig < dest ? 9 : -9;
    if (square::anti_eq(orig, dest)) return orig < dest ? 7 : -7;

    return 0;
}
