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


static size_t gen_pseudo   (MoveList& moves, const Position& pos, bool tactical);
static size_t gen_evasions (MoveList& moves, const Position& pos);

size_t gen_moves_local(MoveList& moves, const Position& pos, GenMode mode)
{
    assert(!pos.checkers());
    assert(moves.empty());

    gen_pseudo(moves, pos, mode == GenMode::Tactical);

    if (mode == GenMode::Legal) {
        int ksq = pos.king();

        for (size_t i = 0; i < moves.size(); i++) {
            Move m = moves[i];

            int orig = m.orig();

            bool validate = bb::test(pos.pins(), orig) || orig == ksq || m.is_ep();

            if (validate && !pos.move_is_legal(m))
                moves.remove_at(i--);
        }
    }

    return moves.size();
}

void gen_pawn(MoveList& moves, const Position& pos, bool tactical, u64 targets)
{
    side_t side = pos.side();

    int incr = square::incr(side);

    u64 occ = pos.occ();
    u64 obb = pos.bb(side_t(!side));
    u64 pbb = pos.bb(side, Pawn);

    // Captures

    for (u64 bb = pbb; bb; ) {
        int psq = bb::pop(bb);

        u64 patt = PawnAttacks[side][psq] & targets & obb;

        while (patt) {
            int csq = bb::pop(patt);

            Move m(psq, csq, pos.board(csq));
           
            if (bb::test(bb::PromoRanks, csq)) {
                moves.add(m | Move::PromoQFlag);

                if (!tactical) {
                    moves.add(m | Move::PromoRFlag);
                    moves.add(m | Move::PromoBFlag);
                    moves.add(m | Move::PromoNFlag);
                }
            }
            else
                moves.add(m);
        }
    }

    // Single pushes

    for (u64 bb = bb::PawnSingles(pbb, ~occ, side) & targets; bb; ) {
        int psq = bb::pop(bb);

        if (bb::test(bb::PromoRanks, psq)) {
            Move m(psq - incr, psq);
            
            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }
        else if (!tactical)
            moves.add(Move(psq - incr, psq) | Move::SingleFlag);
    }

    // En passant

    if (int ep_sq = pos.ep_sq(); ep_sq != square::None) {
        int epdual = square::ep_dual(ep_sq);

        int file = square::file(epdual);

        if (file != FileA && bb::test(pbb, epdual - 1)) {
            Move m(epdual - 1, ep_sq, make_pawn(!side));
            moves.add(m | Move::EPFlag);
        }

        if (file != FileH && bb::test(pbb, epdual + 1)) {
            Move m(epdual + 1, ep_sq, make_pawn(!side));
            moves.add(m | Move::EPFlag);
        }
    }

    // Double pushes

    if (!tactical) {
        for (u64 bb = bb::PawnDoubles(pbb, ~occ, side) & targets; bb; ) {
            int psq = bb::pop(bb);

            moves.add(Move(psq - 2 * incr, psq) | Move::DoubleFlag);
        }
    }
}

size_t gen_pseudo(MoveList& moves, const Position& pos, bool tactical)
{
    assert(!pos.checkers());

    // Pawn

    gen_pawn(moves, pos, tactical, bb::Full);

    side_t side = pos.side();

    int ksq = pos.king(side);

    u64 occ = pos.occ();
    u64 qbb = pos.bb(side, Queen);

    u64 targets = tactical ? pos.bb(side_t(!side)) : ~pos.bb(side_t(side));

    // Knight

    for (u64 bb = pos.bb(side, Knight); bb; ) {
        int psq = bb::pop(bb);

        u64 natt = bb::Leorik::Knight(psq, targets);

        while (natt) {
            int csq = bb::pop(natt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // Bishop and Queen

    for (u64 bb = pos.bb(side, Bishop) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            int csq = bb::pop(batt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // Rook and Queen

    for (u64 bb = pos.bb(side, Rook) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            int csq = bb::pop(ratt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // King

    for (u64 bb = bb::Leorik::King(ksq, targets); bb; ) {
        int csq = bb::pop(bb);

        moves.add(Move(ksq, csq, pos.board(csq)));
    }

    // Castling

    if (!tactical) {
        if (pos.can_castle_k()) {
            u64 between = bb::bit(ksq + 1) | bb::bit(ksq + 2);

            if ((between & occ) == 0)
                moves.add(Move(ksq , ksq + 2) | Move::CastleFlag);
        }

        if (pos.can_castle_q()) {
            u64 between = bb::bit(ksq - 1) | bb::bit(ksq - 2) | bb::bit(ksq - 3);

            if ((between & occ) == 0)
                moves.add(Move(ksq, ksq - 2) | Move::CastleFlag);
        }
    }

    return moves.size();
}

size_t gen_evasions(MoveList& moves, const Position& pos)
{
    assert(pos.checkers());

    side_t side = pos.side();

    const int ksq = pos.king();

    // These are the same square if there is only a single checker

    int checker1 = pos.checker1();
    int checker2 = pos.checker2();

    u64 between = bb::Between[ksq][checker1];

    u64 occ = pos.occ();
    u64 mbb = pos.bb(side);
    u64 qbb = pos.bb(side, Queen);

    if (checker1 != checker2) {
        between |= bb::Between[ksq][checker2];

        u64 targets = ~(mbb | between);

        Position tmp = pos;

        for (u64 bb = bb::Leorik::King(ksq, targets); bb; ) {
            int sq = bb::pop(bb);

            Move m(ksq, sq, pos.board(sq));

            if (tmp.move_is_sane(m))
                moves.add(m);
        }

        return moves.size();
    }

    // King

    for (u64 bb = bb::Leorik::King(ksq, ~(mbb | between)); bb; ) {
        int sq = bb::pop(bb);

        moves.add(Move(ksq, sq, pos.board(sq)));
    }

    u64 targets = bb::bit(checker1) | between;

    // Pawn

    gen_pawn(moves, pos, false, targets);

    // Knight

    for (u64 bb = pos.bb(side, Knight); bb; ) {
        int psq = bb::pop(bb);

        u64 natt = bb::Leorik::Knight(psq, targets);

        while (natt) {
            int csq = bb::pop(natt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // Bishop and Queen

    for (u64 bb = pos.bb(side, Bishop) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            int csq = bb::pop(batt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // Rook and Queen

    for (u64 bb = pos.bb(side, Rook) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            int csq = bb::pop(ratt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    Position tmp = pos;

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig();

        bool validate = bb::test(pos.pins(), orig) || orig == ksq || m.is_ep();

        if (validate && !tmp.move_is_sane(m))
            moves.remove_at(i--);
    }

    return moves.size();
}

// TODO Only direct checks are considered. Eventually add discovered checks?
size_t add_checks(MoveList& moves, const Position& pos)
{
    assert(!pos.checkers());

    side_t side = pos.side();

    int incr = square::incr(side);
    int oksq = pos.king(!side);

    u64 occ = pos.occ();
    u64 pbb = pos.bb(side, Pawn);
    u64 qbb = pos.bb(side, Queen);

    // Pawns

    u64 targets = PawnAttacks[!side][oksq];

    for (u64 bb = bb::PawnSingles(pbb, ~occ, side) & targets; bb; ) {
        int psq = bb::pop(bb);

        moves.add(Move(psq - incr, psq) | Move::SingleFlag);
    }

    for (u64 bb = bb::PawnDoubles(pbb, ~occ, side) & targets; bb; ) {
        int psq = bb::pop(bb);

        moves.add(Move(psq - 2 * incr, psq) | Move::DoubleFlag);
    }

    // Knights

    targets = bb::Leorik::Knight(oksq, ~occ);

    for (u64 bb = pos.bb(side, Knight); bb; ) {
        int psq = bb::pop(bb);

        u64 natt = bb::Leorik::Knight(psq, targets);

        while (natt) {
            int csq = bb::pop(natt);

            moves.add(Move(psq, csq));
        }
    }

    u64 btargets = bb::Leorik::Bishop(oksq, occ) & ~occ;
    u64 rtargets = bb::Leorik::Rook(oksq, occ) & ~occ;

    // Bishop and Queen

    for (u64 bb = pos.bb(side, Bishop) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 batt = BishopAttacks[psq] & btargets;

        while (batt) {
            int csq = bb::pop(batt);

            if ((bb::Between[psq][csq] & occ) == 0)
                moves.add(Move(psq, csq));
        }
    }

    // Rook and Queen

    for (u64 bb = pos.bb(side, Rook) | qbb; bb; ) {
        int psq = bb::pop(bb);

        u64 ratt = RookAttacks[psq] & rtargets;

        while (ratt) {
            int csq = bb::pop(ratt);

            if ((bb::Between[psq][csq] & occ) == 0)
                moves.add(Move(psq, csq));
        }
    }

    return moves.size();
}

size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode)
{
    assert(moves.empty());

#if PROFILE >= PROFILE_ALL
    Timer timer(true);
#endif

    size_t count = pos.checkers()
                 ? gen_evasions(moves, pos)
                 : gen_moves_local(moves, pos, mode);

#if PROFILE >= PROFILE_SOME
    gstats.moves_max = max(gstats.moves_max, int(count));
#endif
#if PROFILE >= PROFILE_ALL
    timer.stop();

    if (pos.checkers()) mode = GenMode::Legal;

    gstats.calls_gen[size_t(mode)]++;
    gstats.cycles_gen[size_t(mode)] += timer.elapsed_cycles();
    gstats.time_gen_ns += timer.elapsed_time<Timer::Nano>();
#endif

    return count;
}

GenState gen_state(const Position& pos)
{
    MoveList moves;

    size_t count = pos.checkers()
                 ? gen_evasions(moves, pos)
                 : gen_moves_local(moves, pos, GenMode::Legal);

    if (count)
        return GenState::Normal;
    else if (pos.checkers())
        return GenState::Checkmate;
    else
        return GenState::Stalemate;
}

int delta_incr(int orig, int dest)
{
    assert(orig != dest);
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));

    if (square::rank_eq(orig, dest)) return orig < dest ? 1 : -1;
    if (square::file_eq(orig, dest)) return orig < dest ? 8 : -8;
    if (square::diag_eq(orig, dest)) return orig < dest ? 9 : -9;
    if (square::anti_eq(orig, dest)) return orig < dest ? 7 : -7;

    return 0;
}
