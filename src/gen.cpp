#include <iomanip>
#include <iostream>
#include "gen.h"
#include "move.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "square.h"
#include "types.h"

using namespace std;

constexpr int DeltaCount  = 240;
constexpr int DeltaOffset = 120;

static i8 DeltaIncr[DeltaCount];
static u8 DeltaType[DeltaCount];

u8 CastleLUT[128];

template <GenMode mode>
static size_t gen_moves(MoveList& moves, const Position& pos);

static size_t gen_evasions(MoveList& moves, const Position& pos);

template <GenMode mode>
static void gen_pawn(MoveList& moves, const Position& pos, const int orig);

template <GenMode mode>
static void gen_knight(MoveList& moves, const Position& pos, const int orig);

template <GenMode mode>
static void gen_moves_slider(MoveList& moves, const Position& pos, const int orig, const int (&Incrs)[4]);

template <GenMode mode>
static void gen_king(MoveList& moves, const Position& pos);

static void gen_piece   (MoveList& moves, const Position& pos, const int dest, const bitset<128>& pins);
static void gen_pawn    (MoveList& moves, const Position& pos, const int dest, const bitset<128>& pins);
static void gen_pinned  (MoveList& moves, const Position& pos, bitset<128>& pins);
static void gen_pinned  (MoveList& moves, const Position& pos, int attacker, int pinned, int incr);

void gen_init()
{
    DeltaType[DeltaOffset + 16 - 1] |= WhitePawnFlag256;
    DeltaType[DeltaOffset + 16 + 1] |= WhitePawnFlag256;
    DeltaType[DeltaOffset - 16 - 1] |= BlackPawnFlag256;
    DeltaType[DeltaOffset - 16 + 1] |= BlackPawnFlag256;

    for (auto incr : KnightIncrs) {
        DeltaIncr[DeltaOffset + incr] = incr;
        DeltaType[DeltaOffset + incr] |= KnightFlag256;
    }

    for (auto incr : BishopIncrs) {
        for (int i = 1; i <= 7; i++) {
            DeltaIncr[DeltaOffset + incr * i] = incr;
            DeltaType[DeltaOffset + incr * i] |= BishopFlag256;
        }
    }

    for (auto incr : RookIncrs) {
        for (int i = 1; i <= 7; i++) {
            DeltaIncr[DeltaOffset + incr * i] = incr;
            DeltaType[DeltaOffset + incr * i] |= RookFlag256;
        }
    }

    for (auto incr : QueenIncrs)
        DeltaType[DeltaOffset + incr] |= KingFlag256;

    for (int i = 0; i < 128; i++)
        CastleLUT[i] = ~0;

    CastleLUT[A1] = ~Position::WhiteCastleQFlag;
    CastleLUT[E1] = ~(Position::WhiteCastleQFlag | Position::WhiteCastleKFlag);
    CastleLUT[H1] = ~Position::WhiteCastleKFlag;
    CastleLUT[A8] = ~Position::BlackCastleQFlag;
    CastleLUT[E8] = ~(Position::BlackCastleQFlag | Position::BlackCastleKFlag);
    CastleLUT[H8] = ~Position::BlackCastleKFlag;
}

template <GenMode mode>
size_t gen_moves(MoveList& moves, const Position& pos)
{
    constexpr bool legal = mode == GenMode::Legal;
    
    assert(legal || !pos.in_check());

    gen_king<mode>(moves, pos);

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

    return moves.size();
}

size_t gen_moves(MoveList& moves, const Position& pos, GenMode mode)
{
    assert(moves.empty());

    Timer timer(Profile >= ProfileLevel::Medium);

    size_t count;

    if (pos.in_check())
        count = gen_evasions(moves, pos);
    else if (mode == GenMode::Tactical)
        count = gen_moves<GenMode::Tactical>(moves, pos);
    else if (mode == GenMode::Pseudo)
        count = gen_moves<GenMode::Pseudo>(moves, pos);
    else 
        count = gen_moves<GenMode::Legal>(moves, pos);

    if (timer.stop()) {
        gstats.time_gen_ns += timer.elapsed_time<Timer::Nano>();
        gstats.cycles_gen += timer.elapsed_cycles();
    }

    return count;
}

template <GenMode mode>
void gen_pawn(MoveList& moves, const Position& pos, const int orig)
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));
    assert(pos.is_piece(orig, Pawn));

    constexpr bool tactical = mode == GenMode::Tactical;

    u8 oflag = make_flag(flip_side(pos.side()));
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

                if (mode != GenMode::Legal || pos.move_is_legal_ep(m))
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
void gen_knight(MoveList& moves, const Position& pos, const int orig)
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));
    assert(pos.is_piece(orig, Knight));

    u8 oflag = make_flag(flip_side(pos.side()));

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
void gen_slider(MoveList& moves, const Position& pos, const int orig, const int (&Incrs)[4])
{
    assert(sq88_is_ok(orig));
    assert(pos.is_me(orig));

    u8 oflag = make_flag(flip_side(pos.side()));

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
    int king = pos.king_sq();

    assert(pos.is_me(king));
    assert(pos.is_piece(king, King));

    constexpr bool tactical = mode == GenMode::Tactical;
    constexpr bool legal    = mode == GenMode::Legal;

    side_t oside = flip_side(pos.side());
    u8 oflag = make_flag(oside);

    for (auto incr : QueenIncrs) {
        int dest = king + incr;
        u8 piece = pos[dest];

        if (tactical) {
            if ((piece & oflag) && !pos.side_attacks(oside, dest))
                moves.add(Move(king, dest, piece));
        }
        else if (piece == PieceNone256 || (piece & oflag)) {
            if (!legal || !pos.side_attacks(oside, dest))
                moves.add(Move(king, dest, piece));
        }
    }

    if (!tactical) {
        
        // Never castle over check but allow castling into check and
        // validate before the move is made. It's hardly a performance
        // improvement, but always validating alters the tree for some
        // reason, so for now it's left in place.

        if (pos.can_castle_k()) {
            int king1 = king + 1;
            int king2 = king + 2;

            if (pos.empty(king1) && pos.empty(king2)) {
                bool ok = !pos.side_attacks(oside, king1)
                    && (!legal || !pos.side_attacks(oside, king2));

                if (ok) moves.add(Move(king, king2) | Move::CastleFlag);
            }
        }

        if (pos.can_castle_q()) {
            int king1 = king - 1;
            int king2 = king - 2;
            int king3 = king - 3;

            if (pos.empty(king1) && pos.empty(king2) && pos.empty(king3)) {
                bool ok = !pos.side_attacks(oside, king1)
                    && (!legal || !pos.side_attacks(oside, king2));

                if (ok) moves.add(Move(king, king2) | Move::CastleFlag);
            }
        }
    }
}

size_t gen_evasions(MoveList& moves, const Position& pos)
{
    int king = pos.king_sq();

    assert(pos.in_check());

    int checker1 = pos.checkers(0);
    // HACK: silence compiler warning
    int checker2 = pos.checkers() > 1 ? (int)pos.checkers(1) : (int)SquareNone;

    int incr1 = is_slider(pos[checker1]) ? delta_incr(king, checker1) : 0;
    int incr2 = checker2 != SquareNone && is_slider(pos[checker2]) ? delta_incr(king, checker2) : 0;

    side_t mside = pos.side();
    side_t oside = flip_side(pos.side());

    u8 oflag = make_flag(oside);

    for (auto incr : QueenIncrs) {
        if (incr == -incr1 || incr == -incr2)
            continue;

        int dest = king + incr;
        u8 piece = pos[dest];

        if (piece == PieceNone256 || (piece & oflag))
            if (!pos.side_attacks(oside, dest))
                moves.add(Move(king, dest, piece));
    }

    if (pos.checkers() == 2)
        return moves.size();

    bitset<128> pins;

    pos.mark_pins(pins);

    u8 mpawn = make_pawn(mside);
    u8 opawn = make_pawn(oside);
    int rank = sq88_rank(checker1, mside);
    int pincr = pawn_incr(mside);

    // pawn captures checking piece

    if (checker1 == ep_dual(pos.ep_sq())) {
        if (int orig = checker1 - 1; pos[orig] == mpawn && !pins.test(orig))
            moves.add(Move(orig, pos.ep_sq(), opawn) | Move::EPFlag);
        if (int orig = checker1 + 1; pos[orig] == mpawn && !pins.test(orig))
            moves.add(Move(orig, pos.ep_sq(), opawn) | Move::EPFlag);
    }

    if (int orig = checker1 - pincr - 1; pos[orig] == mpawn && !pins.test(orig)) {
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

    if (int orig = checker1 - pincr + 1; pos[orig] == mpawn && !pins.test(orig)) {
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

    for (auto orig : pos.plist(WN12 + mside)) {
        if (pins.test(orig))
            continue;

        if (pseudo_attack(orig, checker1, KnightFlag256))
            moves.add(Move(orig, checker1, pos[checker1]));
    }

    for (const auto &p : P12Flag) {
        for (int orig : pos.plist(p.first + mside)) {
            if (pins.test(orig))
                continue;

            if (!pseudo_attack(orig, checker1, p.second))
                continue;

            if (pos.ray_is_empty(orig, checker1))
                moves.add(Move(orig, checker1, pos[checker1]));
        }
    }

    // blockers if slider

    if (is_slider(pos[checker1])) {
        for (int sq = king + incr1; sq != checker1; sq += incr1) {
            gen_pawn(moves, pos, sq, pins);
            gen_piece(moves, pos, sq, pins);
        }
    }

    return moves.size();
}

void gen_piece(MoveList& moves, const Position& pos, const int dest, const bitset<128>& pins)
{
    side_t mside = pos.side();

    for (int orig : pos.plist(WN12 + mside)) {
        if (pins.test(orig))
            continue;

        if (pseudo_attack(orig, dest, KnightFlag256))
            moves.add(Move(orig, dest, pos[dest]));
    }

    for (const auto &p : P12Flag) {
        for (int orig : pos.plist(p.first + mside)) {
            if (pins.test(orig))
                continue;

            if (!pseudo_attack(orig, dest, p.second))
                continue;

            if (pos.ray_is_empty(orig, dest))
                moves.add(Move(orig, dest, pos[dest]));
        }
    }
}

void gen_pawn(MoveList& moves, const Position& pos, const int dest, const bitset<128>& pins)
{
    side_t mside = pos.side();
    int pincr = pawn_incr(mside);
    int rank = sq88_rank(dest, mside);
    u8 mpawn = make_pawn(mside);

    int orig;

    // double push

    if (rank == Rank4) {
        orig = dest - pincr - pincr;

        if (pos[orig] == mpawn && !pins.test(orig))
            if (pos[orig + pincr] == PieceNone256)
                moves.add(Move(orig, dest) | Move::DoubleFlag);
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
    int king = pos.king_sq();

    assert(sq88_is_ok(attacker));
    assert(sq88_is_ok(pinned));
    assert(sq88_is_ok(king));
    assert(incr != 0);
    assert(pos.is_op(attacker));
    assert(pos.is_me(pinned));
    assert(pos.king_sq() == king);
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

        for (sq = king - incr; pos.empty(sq); sq -= incr)
            moves.add(Move(pinned, sq));
        for (sq = pinned - incr; pos.empty(sq); sq -= incr)
            moves.add(Move(pinned, sq));

        moves.add(Move(pinned, attacker, opiece));
    }
}

void gen_pinned(MoveList& moves, const Position& pos, bitset<128>& pins)
{
    assert(pins.none());

    int king = pos.king_sq();
    side_t oside = flip_side(pos.side());
    u8 mflag = make_flag(pos.side());

    for (const auto &p : P12Flag) {
        for (auto attacker : pos.plist(p.first + oside)) {
            if (!pseudo_attack(attacker, king, p.second))
                continue;

            int incr = delta_incr(attacker, king);
            int sq = attacker + incr;

            while (pos[sq] == PieceNone256)
                sq += incr;

            if ((pos[sq] & mflag) == 0)
                continue;

            int pinned = sq;

            sq = king - incr;

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

    size_t count = gen_moves(moves, pos, GenMode::Legal);

    if (count > 0)
        return GenState::Normal;
    else if (pos.in_check())
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
