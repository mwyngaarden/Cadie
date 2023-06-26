#include <iomanip>
#include <iostream>
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "square.h"

using namespace std;


template <GenMode> static size_t gen_moves  (MoveList&, const Position&);
template <GenMode> static void gen_pawn     (MoveList&, const Position&, int o);
template <GenMode> static void gen_knight   (MoveList&, const Position&, int o);
template <GenMode> static void gen_slider   (MoveList&, const Position&, int o, const int (&Incrs)[4]);
template <GenMode> static void gen_king     (MoveList&, const Position&);

static void gen_piece   (MoveList&, const Position&, int d, bitset<128>& pins);
static void gen_pawn    (MoveList&, const Position&, int d, bitset<128>& pins);
static void gen_pinned  (MoveList&, const Position&, bitset<128>& pins);
static void gen_pinned  (MoveList&, const Position&, int attacker, int pinned, int incr);

template <GenMode mode>
size_t gen_moves(MoveList& moves, const Position& pos)
{
    constexpr bool legal = mode == GenMode::Legal;
    
    assert(legal || !pos.checkers());

    bitset<128> pins;

    if (legal)
        gen_pinned(moves, pos, pins);

    side_t side = pos.side();

    for (auto orig : pos.plist(WP12 + side)) {
        if (legal && pins.test(orig)) continue;

        gen_pawn<mode>(moves, pos, orig);
    }

    for (auto orig : pos.plist(WN12 + side)) {
        if (legal && pins.test(orig)) continue;

        gen_knight<mode>(moves, pos, orig);
    }

    for (auto orig : pos.plist(WB12 + side)) {
        if (legal && pins.test(orig)) continue;

        gen_slider<mode>(moves, pos, orig, BishopIncrs);
    }

    for (auto orig : pos.plist(WR12 + side)) {
        if (legal && pins.test(orig)) continue;

        gen_slider<mode>(moves, pos, orig, RookIncrs);
    }

    for (auto orig : pos.plist(WQ12 + side)) {
        if (legal && pins.test(orig)) continue;

        gen_slider<mode>(moves, pos, orig, BishopIncrs);
        gen_slider<mode>(moves, pos, orig, RookIncrs);

    }

    gen_king<mode>(moves, pos);

    return moves.size();
}

size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode)
{
    assert(moves.empty());

#if PROFILE >= PROFILE_ALL
    Timer timer(true);
#endif

    size_t count;

    if (mode == GenMode::Tactical) {
        assert(!pos.checkers());
        count = gen_moves<GenMode::Tactical>(moves, pos);
    }

    else if (pos.checkers())
        count = gen_evas(moves, pos);

    else if (mode == GenMode::Pseudo)
        count = gen_moves<GenMode::Pseudo>(moves, pos);

    else {
        assert(mode == GenMode::Legal);
        count = gen_moves<GenMode::Legal>(moves, pos);
    }

#if PROFILE >= PROFILE_ALL
    timer.stop();

    if (pos.checkers()) mode = GenMode::Legal;

    gstats.calls_gen[size_t(mode)]++;
    gstats.cycles_gen[size_t(mode)] += timer.elapsed_cycles();
    gstats.time_gen_ns += timer.elapsed_time<Timer::Nano>();

    gstats.amoves_max = max(gstats.amoves_max, int(count));
#endif

    return count;
}

template <GenMode mode>
void gen_pawn(MoveList& moves, const Position& pos, int orig)
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));
    assert(pos.is_piece(orig, Pawn));

    constexpr bool tactical = mode == GenMode::Tactical;
    constexpr bool legal    = mode == GenMode::Legal;

    u8 oflag = make_flag(!pos.side());
    int pincr = pawn_incr(pos.side());
    int rank = sq88_rank(orig, pos.side());

    assert(rank >= Rank2 && rank <= Rank7);

    // promotions

    if (rank == Rank7) {
        int dest = orig + pincr - 1;
       
        // capture and promote left

        if (pos[dest] & oflag) {
            Move m(orig, dest, pos[dest]);
            
            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }

        dest++;

        // promote without capture
       
        if (pos.empty(dest)) {
            Move m(orig, dest);

            moves.add(m | Move::PromoQFlag);

            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }

        dest++;

        // capture and promote right

        if (pos[dest] & oflag) {
            Move m(orig, dest, pos[dest]);

            moves.add(m | Move::PromoQFlag);
            
            if (!tactical) {
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            }
        }
    } 
    else {
        // en passant

        if (int ep_sq = pos.ep_sq(); ep_sq != SquareNone) {
            int epdual = ep_dual(ep_sq);

            if (orig == epdual - 1 || orig == epdual + 1) {
                Move m = Move(orig, ep_sq, pos[epdual]) | Move::EPFlag;

                if (!legal || pos.move_is_legal_ep(m))
                    moves.add(m);
            }
        }

        // boring ol' captures

        if (int dest = orig + pincr - 1; pos[dest] & oflag)
            moves.add(Move(orig, dest, pos[dest]));
        if (int dest = orig + pincr + 1; pos[dest] & oflag)
            moves.add(Move(orig, dest, pos[dest]));

        // single push

        if (!tactical) {
            if (int dest = orig + pincr; pos.empty(dest)) {
                moves.add(Move(orig, dest));

                // double push

                dest += pincr;

                if (rank == Rank2 && pos.empty(dest))
                    moves.add(Move(orig, dest) | Move::DoubleFlag);
            }
        }
    }
}

template <GenMode mode>
void gen_knight(MoveList& moves, const Position& pos, int orig)
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));
    assert(pos.is_piece(orig, Knight));
    
    u8 oflag = make_flag(!pos.side());

    for (auto incr : KnightIncrs) {
        int dest = orig + incr;
        u8 piece = pos[dest];

        if (mode == GenMode::Tactical) {
            if (piece & oflag)
                moves.add(Move(orig, dest, piece));
        }
        else if (piece == PieceNone256 || (piece & oflag))
            moves.add(Move(orig, dest, piece));
    }
}

template <GenMode mode>
void gen_slider(MoveList& moves, const Position& pos, int orig, const int (&Incrs)[4])
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));
    
    u8 oflag = make_flag(!pos.side());

    for (auto incr : Incrs) {
        u8 piece;

        for (int dest = orig + incr; (piece = pos[dest]) != PieceInvalid256; dest += incr) {
            if (piece == PieceNone256) {
                if (mode != GenMode::Tactical)
                    moves.add(Move(orig, dest));
            }
            else {
                if (piece & oflag)
                    moves.add(Move(orig, dest, piece));

                break;
            }
        }
    }
}

template <GenMode mode>
void gen_king(MoveList& moves, const Position& pos)
{
    int ksq = pos.king();

    assert(pos.is_me(ksq));
    assert(pos.is_piece(ksq, King));

    constexpr bool tactical = mode == GenMode::Tactical;
    constexpr bool legal    = mode == GenMode::Legal;

    side_t oside = !pos.side();
    u8 oflag = make_flag(oside);

    for (auto incr : QueenIncrs) {
        int dest = ksq + incr;
        u8 piece = pos[dest];

        if (tactical) {
            if (piece & oflag)
                moves.add(Move(ksq, dest, piece));
        }
        else if (piece == PieceNone256 || (piece & oflag)) {
            if (!legal || !pos.side_attacks(oside, dest))
                moves.add(Move(ksq, dest, piece));
        }
    }

    if (!tactical) {
        
        // Never castle over check but allow castling into check and
        // validate before the move is made. It's hardly a performance
        // improvement, but always validating alters the tree for some
        // reason, so for now it's left in place.

        if (pos.can_castle_k()) {
            int king1 = ksq + 1;
            int king2 = ksq + 2;

            if (pos.empty(king1) && pos.empty(king2)) {
                if (!pos.side_attacks(oside, king1)) {
                    if (!legal || !pos.side_attacks(oside, king2))
                        moves.add(Move(ksq, king2) | Move::CastleFlag);
                }
            }
        }

        if (pos.can_castle_q()) {
            int king1 = ksq - 1;
            int king2 = ksq - 2;
            int king3 = ksq - 3;

            if (pos.empty(king1) && pos.empty(king2) && pos.empty(king3)) {
                if (!pos.side_attacks(oside, king1)) {
                    if (!legal || !pos.side_attacks(oside, king2))
                        moves.add(Move(ksq, king2) | Move::CastleFlag);
                }
            }
        }
    }
}

size_t gen_evas(MoveList& moves, const Position& pos)
{
    assert(pos.checkers());

    int ksq = pos.king();

    int checker1 = pos.checkers(0);
    // HACK: silence compiler warning
    int checker2 = pos.checkers() > 1 ? (int)pos.checkers(1) : (int)SquareNone;

    int incr1 = is_slider(pos[checker1]) ? delta_incr(ksq, checker1) : 0;
    int incr2 = checker2 != SquareNone && is_slider(pos[checker2]) ? delta_incr(ksq, checker2) : 0;

    side_t mside = pos.side();
    side_t oside = !pos.side();

    u8 oflag = make_flag(oside);

    // king moves
    for (auto incr : QueenIncrs) {
        if (incr == -incr1 || incr == -incr2)
            continue;

        int dest = ksq + incr;
        u8 piece = pos[dest];

        if (piece == PieceNone256 || (piece & oflag)) {
            if (!pos.side_attacks(oside, dest))
                moves.add(Move(ksq, dest, piece));
        }
    }

    if (pos.checkers() == 2)
        return moves.size();

    bitset<128> pins;

    pos.mark_pins(pins);

    u8 mpawn = make_pawn(mside);
    u8 opawn = make_pawn(oside);
    int rank = sq88_rank(checker1, mside);
    int pincr = pawn_incr(mside);

    // pawn captures checking piece by ep

    if (checker1 == ep_dual(pos.ep_sq())) {
        if (int orig = checker1 - 1; pos[orig] == mpawn && !pins.test(orig))
            moves.add(Move(orig, pos.ep_sq(), opawn) | Move::EPFlag);
        if (int orig = checker1 + 1; pos[orig] == mpawn && !pins.test(orig))
            moves.add(Move(orig, pos.ep_sq(), opawn) | Move::EPFlag);
    }

    // pawn captures checking piece

    for (auto offset : { -1, 1 }) {
        int orig = checker1 - pincr + offset;

        if (pos[orig] != mpawn || pins.test(orig)) continue;
        
        Move m(orig, checker1, pos[checker1]);

        if (rank == Rank8) {
            moves.add(m | Move::PromoQFlag);
            moves.add(m | Move::PromoRFlag);
            moves.add(m | Move::PromoBFlag);
            moves.add(m | Move::PromoNFlag);
        } 
        else
            moves.add(m);
    }
    
    // piece captures checking piece

    gen_piece(moves, pos, checker1, pins);

    // blockers if slider

    if (is_slider(pos[checker1])) {
        for (int sq = ksq + incr1; sq != checker1; sq += incr1) {
            gen_pawn(moves, pos, sq, pins);
            gen_piece(moves, pos, sq, pins);
        }
    }

    return moves.size();
}

void gen_piece(MoveList& moves, const Position& pos, int dest, bitset<128>& pins)
{
    side_t mside = pos.side();

    for (auto p12 : { WN12, WB12, WR12, WQ12 })
        for (auto orig : pos.plist(p12 + mside))
            if (!pins.test(orig) && pos.piece_attacks(orig, dest))
                moves.add(Move(orig, dest, pos[dest]));
}

void gen_pawn(MoveList& moves, const Position& pos, int dest, bitset<128>& pins)
{
    side_t mside = pos.side();
    int pincr = pawn_incr(mside);
    int rank = sq88_rank(dest, mside);
    u8 mpawn = make_pawn(mside);

    int orig;

    // double push

    if (rank == Rank4) {
        orig = dest - pincr - pincr;

        if (pos[orig] == mpawn && !pins.test(orig)) {
            if (pos[orig + pincr] == PieceNone256)
                moves.add(Move(orig, dest) | Move::DoubleFlag);
        }
    }

    // single push

    orig = dest - pincr;

    if (pos[orig] == mpawn && !pins.test(orig)) {
        Move m(orig, dest);

        if (rank == Rank8) {
            moves.add(m | Move::PromoQFlag);
            moves.add(m | Move::PromoRFlag);
            moves.add(m | Move::PromoBFlag);
            moves.add(m | Move::PromoNFlag);
        } 
        else
            moves.add(m);
    }
}

void gen_pinned(MoveList& moves, const Position& pos, int attacker, int pinned, int incr)
{
    int ksq = pos.king();

    assert(sq88_is_ok(attacker));
    assert(sq88_is_ok(pinned));
    assert(sq88_is_ok(ksq));
    assert(incr != 0);
    assert(pos.is_op(attacker));
    assert(pos.is_me(pinned));
    assert(pos.king() == ksq);
    assert(is_slider(pos[attacker]));

    u8 mpiece = pos[pinned];
    u8 opiece = pos[attacker];
    int sq;

    if (is_pawn(mpiece)) {
        side_t mside = pos.side();
        int pincr = pawn_incr(mside);
        int rank = sq88_rank(pinned, mside);
        int ep_sq = pos.ep_sq();

        sq = pinned + pincr;

        if (sq - 1 == ep_sq || sq + 1 == ep_sq) {
            if (delta_incr(pinned, ep_sq) == -incr) {
                Move m = Move(pinned, ep_sq, pos[ep_dual(ep_sq)]) | Move::EPFlag;

                if (pos.move_is_legal_ep(m))
                    moves.add(m);
            }
        }

        // vertical pin

        if (abs(incr) == 16) {
            if (pos.empty(sq)) {
                moves.add(Move(pinned, sq));

                sq += pincr;

                if (rank == Rank2 && pos.empty(sq))
                    moves.add(Move(pinned, sq) | Move::DoubleFlag);
            }
        } 

        else if (sq - 1 == attacker || sq + 1 == attacker) {
            Move m(pinned, attacker, opiece);

            if (rank == Rank7) {
                moves.add(m | Move::PromoQFlag);
                moves.add(m | Move::PromoRFlag);
                moves.add(m | Move::PromoBFlag);
                moves.add(m | Move::PromoNFlag);
            } 
            else
                moves.add(m);
        }
    }

    else if (pseudo_attack(pinned, attacker, mpiece)) {
        assert(is_slider(mpiece));
            
        for (sq = ksq - incr; pos.empty(sq); sq -= incr)
            moves.add(Move(pinned, sq));
        for (sq = pinned - incr; pos.empty(sq); sq -= incr)
            moves.add(Move(pinned, sq));

        moves.add(Move(pinned, attacker, opiece));
    }
}

void gen_pinned(MoveList& moves, const Position& pos, bitset<128>& pins)
{
    assert(pins.none());

    int ksq = pos.king();
    side_t oside = !pos.side();
    u8 mflag = make_flag(pos.side());

    for (const auto &p : P12Flag) {
        for (auto attacker : pos.plist(p.first + oside)) {
            if (!pseudo_attack(attacker, ksq, p.second))
                continue;

            int incr = delta_incr(attacker, ksq);
            int sq = attacker + incr;

            while (pos[sq] == PieceNone256)
                sq += incr;

            if ((pos[sq] & mflag) == 0)
                continue;

            int pinned = sq;

            sq = ksq - incr;

            while (pos[sq] == PieceNone256)
                sq -= incr;

            if (sq != pinned)
                continue;

            gen_pinned(moves, pos, attacker, pinned, incr);

            pins.set(pinned);
        }
    }
}

GenState gen_state(const Position& pos)
{
    MoveList moves;

    size_t count = pos.checkers()
                 ? gen_evas(moves, pos)
                 : gen_moves<GenMode::Legal>(moves, pos);

    GenState state;

    if (count)
        state = GenState::Normal;
    else if (pos.checkers())
        state = GenState::Checkmate;
    else
        state = GenState::Stalemate;

    return state;
}

int delta_incr(int orig, int dest)
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));

    return DeltaIncr[DeltaOffset + dest - orig];
}

u8 pseudo_attack(int incr)
{
    assert(incr >= -DeltaOffset && incr < DeltaOffset);

    return DeltaType[DeltaOffset + incr];
}

bool pseudo_attack(int orig, int dest, u8 piece)
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));

    return DeltaType[DeltaOffset + dest - orig] & piece;
}
