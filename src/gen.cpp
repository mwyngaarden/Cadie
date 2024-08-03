#include <iomanip>
#include <iostream>
#include "attack.h"
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

    bool legal    = mode == GenMode::Legal;
    bool tactical = mode == GenMode::Tactical;
    
    assert(moves.empty());

    if (!legal)
        return gen_pseudo(moves, pos, tactical);

    u64 pins = legal ? gen_pins(pos) : 0;

    gen_pseudo(moves, pos, tactical);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig();

        bool validate = bb::test(pins, orig) || is_king(pos.board(orig)) || m.is_ep();

        if (validate && !pos.move_is_legal(m, pins))
            moves.remove_at(i--);
    }

    return moves.size();
}

void gen_pawn(MoveList& moves, const Position& pos, bool tactical, u64 targets)
{
    side_t side = pos.side();

    int incr = square::incr(side);

    u64 mbb = pos.bb(side_t( side));
    u64 obb = pos.bb(side_t(!side));
    u64 occ = mbb | obb;
    u64 bb;

    u64 mpbb = pos.bb(side, Pawn);

    int csq;
    int psq;
    int orig;
    int dest;

    // Captures

    bb = mpbb;
    while (bb) {
        psq = bb::pop(bb);

        u64 patt = PawnAttacks[side][psq] & targets & obb;

        while (patt) {
            csq = bb::pop(patt);

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

    // Singles

    bb = bb::PawnSingles(mpbb, ~occ, side) & targets;
    while (bb) {
        psq = bb::pop(bb);
        orig = psq - incr;

        if (bb::test(bb::PromoRanks, psq)) {
            Move m(orig, psq);
            
            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }
        else if (!tactical)
            moves.add(Move(orig, psq) | Move::SingleFlag);
    }

    // En passant

    if (int ep_sq = pos.ep_sq(); ep_sq != square::SquareCount) {
        int epdual = square::ep_dual(ep_sq);

        int file = square::file(epdual);

        if (file != FileA && bb::test(mpbb, epdual - 1)) {
            Move m = Move(epdual - 1, ep_sq, make_pawn(!side));
            moves.add(m | Move::EPFlag);
        }
        
        if (file != FileH && bb::test(mpbb, epdual + 1)) {
            Move m = Move(epdual + 1, ep_sq, make_pawn(!side));
            moves.add(m | Move::EPFlag);
        }
    }
    
    // Doubles

    if (!tactical) {
        bb = bb::PawnDoubles(mpbb, ~occ, side) & targets;

        while (bb) {
            psq = bb::pop(bb);
            dest = psq;
            orig = dest - 2 * incr;

            moves.add(Move(orig, dest) | Move::DoubleFlag);
        }
    }
}

size_t gen_pseudo(MoveList& moves, const Position& pos, bool tactical)
{
    assert(!pos.checkers());

    side_t side = pos.side();

    u64 mbb = pos.bb(side_t( side));
    u64 obb = pos.bb(side_t(!side));
    u64 occ = mbb | obb;
    u64 targets = tactical ? obb : ~mbb;
    u64 bbb = pos.bb(side, Bishop);
    u64 rbb = pos.bb(side, Rook);
    u64 qbb = pos.bb(side, Queen);

    int csq;
    int psq;

    u64 bb;

    gen_pawn(moves, pos, tactical, bb::Full);

    for (bb = pos.bb(side, Knight); bb; ) {
        psq = bb::pop(bb);

        u64 natt = bb::Leorik::Knight(psq, targets);

        while (natt) {
            csq = bb::pop(natt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }
    
    for (bb = bbb | qbb; bb; ) {
        psq = bb::pop(bb);

        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            csq = bb::pop(batt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }
    
    for (bb = rbb | qbb; bb; ) {
        psq = bb::pop(bb);

        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            csq = bb::pop(ratt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }
   
    const int ksq = pos.king(side);

    for (bb = bb::Leorik::King(ksq, targets); bb; ) {
        csq = bb::pop(bb);

        moves.add(Move(ksq, csq, pos.board(csq)));
    }

    if (!tactical) {
    
        // Never castle over check but allow castling into check and
        // validate before the move is made.

        if (pos.can_castle_k()) {
            u64 between = bb::bit(ksq + 1) | bb::bit(ksq + 2);

            if ((between & occ) == 0) {
                if (!pos.side_attacks(!side, ksq + 1))
                    moves.add(Move(ksq , ksq + 2) | Move::CastleFlag);
            }
        }

        if (pos.can_castle_q()) {
            u64 between = bb::bit(ksq - 1) | bb::bit(ksq - 2) | bb::bit(ksq - 3);

            if ((between & occ) == 0) {
                if (!pos.side_attacks(!side, ksq - 1))
                    moves.add(Move(ksq, ksq - 2) | Move::CastleFlag);
            }
        }
    }

    return moves.size();
}

size_t gen_evasions(MoveList& moves, const Position& pos)
{
    assert(pos.checkers());

    side_t side = pos.side();

    const int ksq = pos.king(side);
    
    int checker1 = pos.checkers(0);
    
    u64 between = bb::InBetween[ksq][checker1];

    u64 mbb = pos.bb(side);
    u64 occ = pos.occ();

    if (pos.checkers() == 2) {
        int checker2 = pos.checkers(1);

        between |= bb::InBetween[ksq][checker2];

        u64 targets = ~(pos.bb(side) | between);

        for (u64 bb = bb::Leorik::King(ksq, targets); bb; ) {
            int sq = bb::pop(bb);

            Position p = pos;

            Move m(ksq, sq, pos.board(sq));

            if (p.make_move_hack(m))
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

    u64 bbb = pos.bb(side, Bishop);
    u64 rbb = pos.bb(side, Rook);
    u64 qbb = pos.bb(side, Queen);

    // Bishop and Queen
    
    for (u64 bb = bbb | qbb; bb; ) {
        int psq = bb::pop(bb);
        
        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            int csq = bb::pop(batt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    // Rook and Queen

    for (u64 bb = rbb | qbb; bb; ) {
        int psq = bb::pop(bb);
        
        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            int csq = bb::pop(ratt);

            moves.add(Move(psq, csq, pos.board(csq)));
        }
    }

    u64 pins = gen_pins(pos);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig();

        bool validate = bb::test(pins, orig) || is_king(pos.board(orig)) || m.is_ep();

        if (validate) {
            Position p = pos;

            if (!p.make_move_hack(m))
                moves.remove_at(i--);
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

#if PROFILE >= PROFILE_ALL
    timer.stop();

    if (pos.checkers()) mode = GenMode::Legal;

    gstats.calls_gen[size_t(mode)]++;
    gstats.cycles_gen[size_t(mode)] += timer.elapsed_cycles();
    gstats.time_gen_ns += timer.elapsed_time<Timer::Nano>();

    gstats.moves_max = max(gstats.moves_max, int(count));
#endif

    return count;
}

u64 gen_pins(const Position& pos)
{
    u64 pins = 0;

    side_t side = pos.side();

    u64 occ = pos.occ();
    u64 mbb = pos.bb(side);
    u64 bbb = pos.bb(!side, Bishop);
    u64 rbb = pos.bb(!side, Rook);
    u64 qbb = pos.bb(!side, Queen);

    int ksq = pos.king();

    for (u64 bb = BishopAttacks[ksq] & (bbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::InBetween[sq][ksq] & occ;

        if ((between & mbb) && bb::single(between))
            pins |= between;
    }
    
    for (u64 bb = RookAttacks[ksq] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::InBetween[sq][ksq] & occ;

        if ((between & mbb) && bb::single(between))
            pins |= between;
    }

    return pins;
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
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));

    return DeltaIncr[DeltaOffset + dest - orig];
}

bool pseudo_attack(int orig, int dest, u8 piece)
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));

    return DeltaType[DeltaOffset + dest - orig] & piece;
}
