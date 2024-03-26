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

static u64 gen_pins        (const Position& pos);

size_t gen_moves_local(MoveList& moves, const Position& pos, GenMode mode)
{
    assert(!pos.checkers());

    bool legal    = mode == GenMode::Legal;
    bool tactical = mode == GenMode::Tactical;
    
    assert(moves.empty());

    if (!legal)
        return gen_pseudo(moves, pos, tactical);

    u64 pinned = gen_pins(pos);

    gen_pseudo(moves, pos, tactical);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig64();

        bool validate = bb::test(pinned, orig) || is_king(pos.board(orig)) || m.is_ep();

        if (validate && !pos.move_is_legal(m))
            moves.remove_at(i--);
    }

    return moves.size();
}

void gen_pawn(MoveList& moves, const Position& pos, bool tactical, u64 targets)
{
    side_t side = pos.side();

    u8 mpawn = make_pawn(side);
    int pincr = pawn_incr(side);

    u64 mbb = pos.occ( side);
    u64 obb = pos.occ(!side);
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

            Move m(to_sq88(psq), to_sq88(csq), pos.board(csq));
           
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
        orig = to_sq88(psq) - pincr;

        if (bb::test(bb::PromoRanks, psq)) {
            Move m(orig, to_sq88(psq));
            
            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }
        else if (!tactical)
            moves.add(Move(orig, to_sq88(psq)));
    }

    // En passant

    if (int ep_sq = pos.ep_sq(); ep_sq != SquareNone) {
        int epdual = square::ep_dual(ep_sq);

        int file = square::file(epdual);

        if (file != FileA && pos.board(epdual - 1) == mpawn) {
            Move m = Move(to_sq88(epdual - 1), to_sq88(ep_sq), make_pawn(!side)) | Move::EPFlag;
            moves.add(m);
        }
        
        if (file != FileH && pos.board(epdual + 1) == mpawn) {
            Move m = Move(to_sq88(epdual + 1), to_sq88(ep_sq), make_pawn(!side)) | Move::EPFlag;
            moves.add(m);
        }
    }
    
    // Doubles

    if (!tactical) {
        bb = bb::PawnDoubles(mpbb, ~occ, side) & targets;

        while (bb) {
            psq = bb::pop(bb);
            dest = to_sq88(psq);
            orig = dest - 2 * pincr;

            moves.add(Move(orig, dest) | Move::DoubleFlag);
        }
    }
}

size_t gen_pseudo(MoveList& moves, const Position& pos, bool tactical)
{
    assert(!pos.checkers());

    side_t side = pos.side();

    u64 mbb = pos.occ(side);
    u64 obb = pos.occ(!side);
    u64 occ = mbb | obb;
    u64 targets = tactical ? obb : ~mbb;

    int csq;
    int psq;

    u64 bb;

    gen_pawn(moves, pos, tactical, bb::Full);

    for (bb = pos.bb(side, Knight); bb; ) {
        psq = bb::pop(bb);

        u64 natt = bb::Leorik::Knight(psq, targets);

        while (natt) {
            csq = bb::pop(natt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }
    
    for (bb = pos.bb(side, Bishop); bb; ) {
        psq = bb::pop(bb);

        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            csq = bb::pop(batt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }
    
    for (bb = pos.bb(side, Rook); bb; ) {
        psq = bb::pop(bb);

        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            csq = bb::pop(ratt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }
    
    for (bb = pos.bb(side, Queen); bb; ) {
        psq = bb::pop(bb);

        u64 qatt = bb::Leorik::Queen(psq, occ) & targets;

        while (qatt) {
            csq = bb::pop(qatt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }

    int ksq   = pos.king(side);
    int ksq64 = to_sq64(ksq);

    for (bb = bb::Leorik::King(ksq64, targets); bb; ) {
        csq = bb::pop(bb);

        moves.add(Move(ksq, to_sq88(csq), pos.board(csq)));
    }

    if (!tactical) {
    
        // Never castle over check but allow castling into check and
        // validate before the move is made.

        if (pos.can_castle_k()) {
            if (pos.empty64(ksq64 + 1) && pos.empty64(ksq64 + 2)) {
                if (!pos.side_attacks(!side, ksq64 + 1))
                    moves.add(Move(ksq, to_sq88(ksq64 + 2)) | Move::CastleFlag);
            }
        }

        if (pos.can_castle_q()) {
            if (pos.empty64(ksq64 - 1) && pos.empty64(ksq64 - 2) && pos.empty64(ksq64 - 3)) {
                if (!pos.side_attacks(!side, ksq64 - 1))
                    moves.add(Move(ksq, to_sq88(ksq64 - 2)) | Move::CastleFlag);
            }
        }
    }

    return moves.size();
}

size_t gen_evasions(MoveList& moves, const Position& pos)
{
    assert(pos.checkers());

    side_t side = pos.side();

    int ksq   = pos.king(side);
    int ksq64 = to_sq64(ksq);
    
    int checker1 = to_sq64(pos.checkers(0));
    
    u64 between = InBetween[ksq64][checker1];

    u64 mbb = pos.occ(side);
    u64 occ = pos.occ();

    if (pos.checkers() == 2) {
        int checker2 = to_sq64(pos.checkers(1));

        between |= InBetween[ksq64][checker2];

        u64 targets = ~(pos.occ(side) | between);

        for (u64 bb = bb::Leorik::King(ksq64, targets); bb; ) {
            int csq = bb::pop(bb);

            Position p = pos;

            Move m(ksq, to_sq88(csq), pos.board(csq));

            if (p.make_move_hack(m, true))
                moves.add(m);
        }

        return moves.size();
    }

    // King

    for (u64 bb = bb::Leorik::King(ksq64, ~(mbb | between)); bb; ) {
        int csq = bb::pop(bb);

        moves.add(Move(ksq, to_sq88(csq), pos.board(csq)));
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

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }

    // Bishop
    
    for (u64 bb = pos.bb(side, Bishop); bb; ) {
        int psq = bb::pop(bb);
        
        //if ((PieceAttacks[Bishop][psq] & targets) == 0) continue;

        u64 batt = bb::Leorik::Bishop(psq, occ) & targets;

        while (batt) {
            int csq = bb::pop(batt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }

    // Rook

    for (u64 bb = pos.bb(side, Rook); bb; ) {
        int psq = bb::pop(bb);
        
        //if ((PieceAttacks[Rook][psq] & targets) == 0) continue;

        u64 ratt = bb::Leorik::Rook(psq, occ) & targets;

        while (ratt) {
            int csq = bb::pop(ratt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }

    // Queen

    for (u64 bb = pos.bb(side, Queen); bb; ) {
        int psq = bb::pop(bb);

        //if ((PieceAttacks[Queen][psq] & targets) == 0) continue;

        u64 qatt = bb::Leorik::Queen(psq, occ) & targets;

        while (qatt) {
            int csq = bb::pop(qatt);

            moves.add(Move(to_sq88(psq), to_sq88(csq), pos.board(csq)));
        }
    }

    u64 pinned = gen_pins(pos);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int orig = m.orig64();

        bool validate = bb::test(pinned, orig) || is_king(pos.board(orig)) || m.is_ep();

        if (validate) {
            Position p = pos;

            if (!p.make_move_hack(m, true))
                moves.remove_at(i--);
        }
    }

    return moves.size();
}

size_t sort_moves(MoveList& moves)
{
    vector<u32> moves32;

    for (auto m : moves) moves32.push_back(m);

    sort(moves32.begin(), moves32.end());

    moves.clear();

    for (auto m : moves32) {
        assert(moves.find(m) == MoveList::npos);
        
        moves.add(m);
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
    u64 pinned = 0;

    int ksq = pos.king64();

    side_t side = pos.side();

    u64 mbb = pos.occ(side);
    u64 occ = pos.occ();

    u64 kbatts = PieceAttacks[Bishop][ksq];
    u64 kratts = PieceAttacks[Rook][ksq];
    u64 kqatts = PieceAttacks[Queen][ksq];

    for (u64 bb = pos.bb(!side, Bishop) & kbatts; bb; ) {
        int sq = bb::pop(bb);

        u64 between = InBetween[sq][ksq] & occ;

        if (between & mbb && bb::single(between))
            pinned |= between;
    }
    
    for (u64 bb = pos.bb(!side, Rook) & kratts; bb; ) {
        int sq = bb::pop(bb);

        u64 between = InBetween[sq][ksq] & occ;

        if (between & mbb && bb::single(between))
            pinned |= between;
    }
    
    for (u64 bb = pos.bb(!side, Queen) & kqatts; bb; ) {
        int sq = bb::pop(bb);

        u64 between = InBetween[sq][ksq] & occ;

        if (between & mbb && bb::single(between))
            pinned |= between;
    }

    return pinned;
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

bool pseudo_attack64(int orig, int dest, u8 piece)
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));

    return pseudo_attack(to_sq88(orig), to_sq88(dest), piece);
}

