#include <algorithm>
#include <array>
#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "pos.h"
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "search.h"
#include "see.h"
#include "string.h"

using namespace std;

Position::Position(const string fen, double outcome)
{
    // init

    for (int i = 0; i < 192; i++)
        square_[i] = PieceInvalid256;

    for (int i = 0; i < 64; i++)
        square(to_sq88(i)) = PieceNone256;

    // fen

    Tokenizer fields(fen);

    assert(fields.size() >= 2);

    const string& pieces     = fields.get(0);
    const string& side       = fields.get(1);
    const string& flags      = fields.get(2, "-");
    const string& ep_sq      = fields.get(3, "-");
    const string& half_moves = fields.get(4, "0");
    const string& full_moves = fields.get(5, "1");

    // piece placement

    int rank = Rank8;
    int file = FileA;

    for (char c : pieces) {
        switch (c) {
        case '/':
            rank--;
            file = 0;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            file += c - '0';
            break;
        default:
            add_piece<true>(to_sq88(file, rank), char_to_piece256(c));
            file++;
            break;
        }
    }

    // active color

    if (side == "w")
        side_ = White;
    else if (side == "b")
        side_ = Black;

    // castling

    if (flags.find('K') != string::npos) flags_ |= WhiteCastleKFlag;
    if (flags.find('Q') != string::npos) flags_ |= WhiteCastleQFlag;
    if (flags.find('k') != string::npos) flags_ |= BlackCastleKFlag;
    if (flags.find('q') != string::npos) flags_ |= BlackCastleQFlag;

    // en passant

    if (ep_sq != "-") {
        int sq = san_to_sq88(ep_sq);

        assert(sq88_is_ok(sq));

        ep_sq_ = ep_is_valid(sq) ? sq : SquareNone;
    }

    // half moves counter

    if (half_moves != "-")
        half_moves_ = stoi(half_moves);

    // full moves counter

    if (full_moves != "-")
        full_moves_ = stoi(full_moves);

    // checkers

    set_checkers_slow();

    // key

    key_ ^= zobrist_castle(flags_);
    key_ ^= zobrist_ep(ep_sq_);

    if (side_ == White)
        key_ ^= zobrist_side();

    outcome_ = outcome;
}

string Position::to_fen() const
{
    ostringstream oss;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;

        for (int file = 0; file < 8; file++) {
            u8 piece = square(to_sq88(file, rank));

            if (piece == PieceNone256)
                empty++;
            else {
                if (empty > 0) {
                    oss << to_string(empty);
                    empty = 0;
                }

                oss << piece256_to_char(piece);
            }
        }

        if (empty > 0)
            oss << to_string(empty);

        if (rank > 0)
            oss << '/';
    }

    oss << ' ';
    oss << (side_ == White ? 'w' : 'b');

    oss << ' ';
    if ((flags_ & WhiteCastleKFlag) != 0) oss << 'K';
    if ((flags_ & WhiteCastleQFlag) != 0) oss << 'Q';
    if ((flags_ & BlackCastleKFlag) != 0) oss << 'k';
    if ((flags_ & BlackCastleQFlag) != 0) oss << 'q';
    if ((flags_ & CastleFlags)      == 0) oss << '-';

    oss << ' ';
    oss << (ep_sq_ == SquareNone ? "-" : sq88_to_san(ep_sq_));

    oss << ' ';
    oss << (int)half_moves_;

    oss << ' ';
    oss << (int)full_moves_;

    return oss.str();
}

Move Position::note_move(const string& s) const
{
    // from_string handles promotion only

    Move m = Move::from_string(s);

    int orig = m.orig();
    int dest = m.dest();

    u8 mpiece = square(orig);
    u8 opiece = square(dest);

    m.set_capture(opiece);

    if (is_pawn(mpiece)) {
        if (abs(orig - dest) == 32) {
            assert(opiece == PieceNone256);
            m.set_flag(Move::DoubleFlag);
        }

        else if (dest == ep_sq_) {
            assert(opiece == PieceNone256);
            m.set_flag(Move::EPFlag);
            m.set_capture(make_pawn(!side_));
        }
    }

    else if (is_king(mpiece)) {
        if (abs(orig - dest) == 2) {
            assert(opiece == PieceNone256);
            m.set_flag(Move::CastleFlag);
        }
    }

    return m;
}

void Position::make_move(const Move& m, UndoMove& undo)
{
    assert(m.dest() != king(!side_));
    assert(m.is_valid());
    assert(!is_king(m.capture_piece()));
    assert(mstack.empty() || m != mstack.back());
    assert(key() == calc_key());

    undo.flags          = flags_;
    undo.ep_sq          = ep_sq_;
    undo.half_moves     = half_moves_;
    undo.full_moves     = full_moves_;
    undo.key            = key_;
    undo.pawn_key       = pawn_key_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers       = checkers_;
    undo.pm             = pm_;

    key_ ^= zobrist_castle(flags_);
    key_ ^= zobrist_ep(ep_sq_);
    key_ ^= zobrist_side();

    int orig  = m.orig();
    int dest  = m.dest();
    int piece = square(orig);
    int pincr = pawn_incr(side_);

    ep_sq_ = SquareNone;

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (m.is_ep()) {
        assert(dest == ep_sq_);

        rem_piece<true>(dest - pincr);
        mov_piece<true>(orig, dest);
    }
    else if (m.is_double()) {
        mov_piece<true>(orig, dest);

        if (ep_is_valid(ep_dual(dest)))
            ep_sq_ = ep_dual(dest);
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        if (m.is_promo()) {
            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, m.promo_piece6()));
        } 
        else
            mov_piece<true>(orig, dest);
    }

    flags_      &= CastleMasks[orig] & CastleMasks[dest];
    half_moves_  = is_pawn(piece) || m.is_capture() ? 0 : half_moves_ + 1;
    full_moves_ += side_;
    side_       ^= 1;

    // update checkers

    checkers_ = 0;

    set_checkers_fast(m);

    pm_ = m;

    key_ ^= zobrist_castle(flags_);
    key_ ^= zobrist_ep(ep_sq_);
    
    assert(!side_attacks(side_, king(!side_)));

    kstack.add(key_);
    mstack.add(m);

#if PROFILE >= PROFILE_SOME
    gstats.moves_count++;
#endif
}

void Position::unmake_move(const Move& m, const UndoMove& undo)
{
    assert(m == mstack.back());
    assert(key() == calc_key());

    flags_          = undo.flags;
    ep_sq_          = undo.ep_sq;
    half_moves_     = undo.half_moves;
    full_moves_     = undo.full_moves;
    key_            = undo.key;
    pawn_key_       = undo.pawn_key;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_       = undo.checkers;
    pm_             = undo.pm;

    int orig = m.orig();
    int dest = m.dest();

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<false>(dest, orig);
        mov_piece<false>(rdest, rorig);
    } 
    else if (m.is_ep()) {
        mov_piece<false>(dest, orig);
        add_piece<false>(ep_dual(dest), make_pawn(side_));
    } 
    else {
        if (m.is_promo()) {
            rem_piece<false>(dest);
            add_piece<false>(orig, make_pawn(!side_));
        } 
        else
            mov_piece<false>(dest, orig);

        if (m.is_capture())
            add_piece<false>(dest, m.capture_piece());
    }

    side_ ^= 1;
    
    kstack.pop_back();
    mstack.pop_back();
    
    assert(key() == calc_key());
}

void Position::make_null(UndoNull& undo)
{
    undo.key            = key_;
    undo.ep_sq          = ep_sq_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers       = checkers_;
    undo.pm             = pm_;

    key_    ^= zobrist_side();
    key_    ^= zobrist_ep(ep_sq_);
    side_   ^= 1;

    ep_sq_  = SquareNone;

    checkers_sq_[0] = SquareNone;
    checkers_sq_[1] = SquareNone;
    checkers_       = 0;
    pm_             = MoveNull;

    assert(key() == calc_key());

    kstack.add(key_);
    mstack.add(MoveNull);

#if PROFILE >= PROFILE_SOME
    gstats.moves_count++;
#endif
}

void Position::unmake_null(const UndoNull& undo)
{
    key_            = undo.key;
    ep_sq_          = undo.ep_sq;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_       = undo.checkers;
    pm_             = undo.pm;

    side_ ^= 1;

    assert(key() == calc_key());

    mstack.pop_back();
    kstack.pop_back();
}

template <bool UpdateKey>
void Position::add_piece(int sq, u8 piece)
{
    assert(sq88_is_ok(sq));
    assert(empty(sq));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    size_t lbound = plist_[p12].lower_bound(sq);

    plist_[p12].insert(lbound, sq);

    if (UpdateKey) {
        key_ ^= zobrist_piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zobrist_piece(p12, sq);
    }

    square(sq) = piece;
}

template <bool UpdateKey>
void Position::rem_piece(int sq)
{
    assert(sq88_is_ok(sq));
    assert(!empty(sq));

    u8 piece = square(sq);

    assert(!is_king(piece));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    plist_[p12].remove(sq);

    if (UpdateKey) {
        key_ ^= zobrist_piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zobrist_piece(p12, sq);
    }

    square(sq) = PieceNone256;
}

template <bool UpdateKey>
void Position::mov_piece(int orig, int dest)
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));
    assert(!empty(orig));
    assert(empty(dest));

    u8 piece = square(orig);

    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    plist_[p12].remove(orig);

    size_t lbound = plist_[p12].lower_bound(dest);

    plist_[p12].insert(lbound, dest);
    
    if (UpdateKey) {
        key_ ^= zobrist_piece(p12, orig);
        key_ ^= zobrist_piece(p12, dest);
        
        if (is_pawn(piece)) {
            pawn_key_ ^= zobrist_piece(p12, orig);
            pawn_key_ ^= zobrist_piece(p12, dest);
        }
    }

    swap(square(orig), square(dest));
}

bool Position::side_attacks(side_t side, int dest) const
{
    assert(side_is_ok(side));
    assert(sq88_is_ok(dest));

    // pawns
    
    u8 pawn = make_pawn(side);
    int incr = pawn_incr(side);

    if (int orig = dest - incr - 1; square(orig) == pawn) return true;
    if (int orig = dest - incr + 1; square(orig) == pawn) return true;

    // knights and sliders

    for (auto p12 : { WN12, WB12, WR12, WQ12 })
        for (auto orig : plist(p12 + side))
            if (piece_attacks(orig, dest))
                return true;

    // king

    if (int ksq = king(side); pseudo_attack(ksq, dest, KingFlag256))
        return true;

    return false;
}

bool Position::piece_attacks(int orig, int dest) const
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));
    assert(!empty(orig));

    u8 piece = square(orig);

    assert(piece256_is_ok(piece));

    if (!pseudo_attack(orig, dest, piece))
        return false;

    return !is_slider(piece) || ray_is_empty(orig, dest);
}

// a la Crafty
string Position::pretty() const
{
    ostringstream oss;

    oss << "FEN: " << to_fen()
        << endl
        << "Key: 0x" << setfill('0') << setw(16) << hex << key_
        << endl
        << endl
        << "    +---+---+---+---+---+---+---+---+"
        << endl;

    for (int rank = 7; rank >= 0; rank--) {
        oss << ' ' << (rank + 1) << "  |";

        for (int file = 0; file <= 7; file++) {
            u8 piece = square(to_sq88(file, rank));

            if (is_white(piece))
                oss << '-' << (char)piece256_to_char(piece) << '-';
            else if (is_black(piece))
                oss << '<' << (char)toupper(piece256_to_char(piece)) << '>';
            else {
                int dark = ~(file ^ rank) & 1;
                oss << ' ' << (dark ? '.' : ' ') << ' ';
            }

            oss << '|';
        }

        oss << endl
            << "    +---+---+---+---+---+---+---+---+"
            << endl;
    }

    oss << "      a   b   c   d   e   f   g   h  "
        << endl;

    return oss.str();
}

u64 Position::calc_key() const
{
    u64 key = 0;

    for (int i = 0; i < 64; i++) {
        int sq = to_sq88(i);
        u8 piece = square(sq);

        if (piece == PieceNone256)
            continue;

        assert(piece256_is_ok(piece));

        int p12 = P256ToP12[piece];

        assert(piece12_is_ok(p12));

        key ^= zobrist_piece(p12, sq);
    }

    key ^= zobrist_castle(flags_);
    key ^= zobrist_ep(ep_sq_);

    if (side_ == White)
        key ^= zobrist_side();

    return key;
}

bool Position::ep_is_valid(int sq) const
{
    assert(sq88_is_ok(sq));
    assert(sq88_rank(sq) == Rank3 || sq88_rank(sq) == Rank6);

    sq = ep_dual(sq);

    assert(is_pawn(square(sq)));

    u8 opawn = flip_pawn(square(sq));

    return square(sq - 1) == opawn || square(sq + 1) == opawn;
}

void Position::mark_pins(bitset<128>& pins) const
{
    assert(pins.none());

    u8 mflag = make_flag(side_);
    int ksq = king();
    
    for (auto p12 : { WB12, WR12, WQ12 }) {
        for (auto orig : plist_[p12 + !side_]) {
            if (!pseudo_attack(orig, ksq, square(orig)))
                continue;

            int incr = delta_incr(ksq, orig);
            int sq1 = ksq + incr;
            u8 piece;

            while ((piece = square(sq1)) == PieceNone256)
                sq1 += incr;

            if ((piece & mflag) == 0)
                continue;

            int sq2 = orig - incr;

            while (square(sq2) == PieceNone256)
                sq2 -= incr;

            if (sq1 == sq2)
                pins.set(sq1);
        }
    }
}

void Position::set_checkers_slow()
{
    assert(checkers_ == 0);

    int ksq = king();
    side_t opawn = make_pawn(!side_);
    int pincr = pawn_incr(!side_);

    if (int orig = ksq - pincr - 1; square(orig) == opawn) checkers_sq_[checkers_++] = orig;
    if (int orig = ksq - pincr + 1; square(orig) == opawn) checkers_sq_[checkers_++] = orig;

    for (auto p12 : { WN12, WB12, WR12, WQ12 })
        for (int orig : plist_[p12 + !side_])
            if (piece_attacks(orig, ksq))
                checkers_sq_[checkers_++] = orig;
}

void Position::set_checkers_fast(const Move& m)
{
    assert(checkers_ == 0);

    int ksq  = king();
    int orig = m.orig();
    int dest = m.dest();

    int sq;

    if (m.is_castle()) {
        int rook = dest > orig ? orig + 1 : orig - 1;

        if (pseudo_attack(ksq, rook, RookFlag256)) {
            int incr = delta_incr(ksq, rook);

            sq = ksq + incr;

            while (square(sq) == PieceNone256)
                sq += incr;

            if (sq == rook)
                checkers_sq_[checkers_++] = rook;
        }

        return;
    }

    if (m.is_ep()) {
        set_checkers_slow();

        return;
    }

    int oincr = delta_incr(ksq, orig);
    int dincr = delta_incr(ksq, dest);
    
    u8 piece;

    // revealed check?

    if (oincr != dincr) {
        if (pseudo_attack(oincr) & QueenFlags256) {
            sq = ksq + oincr;

            while ((piece = square(sq)) == PieceNone256)
                sq += oincr;

            u8 oflag = make_flag(!side_);

            if ((piece & oflag) && pseudo_attack(sq, ksq, piece))
                checkers_sq_[checkers_++] = sq;
        }
    }

    // direct check?

    piece = square(dest);

    if (pseudo_attack(dest, ksq, piece)) {
        if (is_slider(piece)) {
            sq = ksq + dincr;

            while (square(sq) == PieceNone256)
                sq += dincr;

            if (sq != dest)
                return;
        }

        checkers_sq_[checkers_++] = dest;
    }
}

bool Position::ray_is_empty(int orig, int dest) const
{
    assert(sq88_is_ok(orig));
    assert(sq88_is_ok(dest));
    assert(pseudo_attack(orig, dest, QueenFlags256));

    int incr = delta_incr(orig, dest);

    do {
        orig += incr;

        if (orig == dest) return true;
    } while (square(orig) == PieceNone256);

    return false;
}

bool Position::move_is_check(const Move& m) const
{
    u8 mflag = make_flag(side_);

    int oksq = king(!side_);
    int orig = m.orig();
    int dest = m.dest();
            
    u8 mpiece = square(orig);
    u8 piece;

    int sq;
    int incr;

#if PROFILE >= PROFILE_SOME
    gstats.ctests++;
#endif

    // En passant moves are rare and a tedious to determine if they give check
    // or not so maintain your sanity and just copy the board and make the
    // move. This will look for direct and discovered checks.
    
    if (m.is_ep()) {
        Position p = *this;
        UndoMove undo;

        p.make_move(m, undo);
        bool move_checks = p.checkers();
        p.unmake_move(m, undo);

        return move_checks;
    }

    else if (is_pawn(mpiece)) {
        if (pseudo_attack(dest, oksq, mpiece))
            return true;

        if (m.is_promo()) {
            u8 promo = to_piece256(side_, m.promo_piece6());

            if (pseudo_attack(dest, oksq, promo)) {
                if (is_knight(promo))
                    return true;

                assert(is_slider(promo));
                
                incr = delta_incr(oksq, dest);
                sq = oksq;

                do {
                    sq += incr;

                    if (sq == dest) return true;
                } while (sq == orig || square(sq) == PieceNone256);
            }
        }

        // Discovered checks not possible?
        if (!m.is_capture() && sq88_file(orig) == sq88_file(oksq))
            return false;
    }

    else if (is_king(mpiece)) {
        if (m.is_castle()) {
            int rdest = dest > orig ? orig + 1 : orig - 1;

            if (!pseudo_attack(rdest, oksq, RookFlag256))
                return false;

            incr = delta_incr(rdest, oksq);
            sq = rdest;

            do { sq += incr; } while (square(sq) == PieceNone256);

            return sq == oksq;
        }

        // Discovered checks not possible?
        if (delta_incr(oksq, orig) == delta_incr(oksq, dest))
            return false;
    }

    else if (is_knight(mpiece)) {
        if (pseudo_attack(oksq, dest, KnightFlag256))
            return true;
    }

    else if (is_slider(mpiece)) {
        if (pseudo_attack(dest, oksq, mpiece)) {
            incr = delta_incr(oksq, dest);
            sq = oksq;

            do { sq += incr; } while (sq != dest && square(sq) == PieceNone256);

            if (sq == dest)
                return true;
        }
    }

    // Is there a discovered check?

    if (!pseudo_attack(orig, oksq, QueenFlags256))
        return false;

    incr = delta_incr(oksq, orig);
    sq = oksq;

    do { sq += incr; } while (sq == orig || (piece = square(sq)) == PieceNone256);

    return (piece & mflag) && pseudo_attack(sq, oksq, piece);
}

bool Position::move_is_legal(const Move& m) const
{
    // The move generator only generates legal evasions when in check

    if (checkers_ > 0) return true;

    int ksq  = king();
    int orig = m.orig();
    int dest = m.dest();

    // At move generation we validated that we are not castling over check, but
    // not if we were castling into check. See note in move generation.
    if (orig == ksq)
        return !side_attacks(!side_, dest);
    
    if (m.is_ep())
        return move_is_legal_ep(m);

    if (!pseudo_attack(orig, ksq, QueenFlags256))
        return true;

    if (!ray_is_empty(orig, ksq))
        return true;

    int kincr = delta_incr(ksq, orig);
    int mincr = delta_incr(orig, dest);

    if (abs(mincr) == abs(kincr))
        return true;

    u8 oflag = make_flag(!side_);
    u8 piece;

    do { orig += kincr; } while ((piece = square(orig)) == PieceNone256);

    return (piece & oflag) == 0 || !pseudo_attack(ksq, orig, piece);
}

bool Position::move_is_legal_ep(const Move& m) const
{
    assert(m.is_ep());
    assert(checkers_ == 0);

    int ksq = king();

    int orig = m.orig();
    int dest = m.dest();
    int pincr = pawn_incr(side_);
    int cap = dest - pincr;
    u8 oflag = make_flag(!side_);
    int krank = sq88_rank(ksq);
    int orank = sq88_rank(orig);
    int oincr = delta_incr(ksq, orig);
    int mincr = orig - dest;
    int cincr = delta_incr(ksq, cap);

    assert(abs(mincr) == 15 || abs(mincr) == 17);

    u8 piece;

    //  king on same rank of capturing and captured pawn?

    if (krank == orank) {
        assert(oincr == cincr);

        int sq = ksq;

        // skip both pawns on rank

        do {
            sq += oincr;
        } while (sq == orig || sq == cap || (piece = square(sq)) == PieceNone256);

        // revealed checks not possible on any file or diagonal

        return (piece & oflag) == 0 || !pseudo_attack(ksq, sq, piece);
    }

    // king on same diagonal of capturing pawn?

    if (pseudo_attack(ksq, orig, BishopFlag256)) {
        // same diagonal of capture line?

        if (oincr == mincr || oincr == -mincr)
            return true;

        if (!ray_is_empty(ksq, orig))
            return true;

        int sq = orig;

        do { sq += oincr; } while ((piece = square(sq)) == PieceNone256);

        if ((piece & oflag) && (piece & BishopFlag256))
            return false;
    }

    // king on same diagonal of captured pawn?

    if (pseudo_attack(ksq, cap, BishopFlag256)) {
        if (!ray_is_empty(ksq, cap))
            return true;

        int sq = cap;

        do { sq += cincr; } while ((piece = square(sq)) == PieceNone256);

        if ((piece & oflag) && (piece & BishopFlag256))
            return false;
    }

    int kfile = sq88_file(ksq);
    int ofile = sq88_file(orig);

    //  king on same file of capturing pawn?

    if (kfile == ofile) {
        assert(abs(oincr) == 16);

        if (!ray_is_empty(ksq, orig))
            return true;

        int sq = orig;

        do { sq += oincr; } while ((piece = square(sq)) == PieceNone256);

        if ((piece & oflag) && (piece & RookFlag256))
            return false;
    }

    return true;
}

int Position::phase() const
{
    int phase = PhaseMax;

    phase -= plist(WN12).size() * PhaseKnight;
    phase -= plist(WB12).size() * PhaseBishop;
    phase -= plist(WR12).size() * PhaseRook;
    phase -= plist(WQ12).size() * PhaseQueen;

    phase -= plist(BN12).size() * PhaseKnight;
    phase -= plist(BB12).size() * PhaseBishop;
    phase -= plist(BR12).size() * PhaseRook;
    phase -= plist(BQ12).size() * PhaseQueen;

    return max(0, phase);
}

void Position::swap_sides()
{
    key_  ^= zobrist_side();
    side_ ^= 1;
}

bool Position::draw_mat_insuf(side_t side) const
{
    if (!plist(WP12 + side).empty() || !plist(WR12 + side).empty() || !plist(WQ12 + side).empty())
        return false;

    if (!plist(WN12 + side).empty() && !plist(WB12 + side).empty())
        return false;

    if (plist(WB12 + side).empty())
        return plist(WN12 + side).size() < 3;

    if (plist(WN12 + side).empty())
        return plist(WB12 + side).size() < 2;

    return true;
}

bool Position::draw_mat_insuf() const
{
    return draw_mat_insuf(White) && draw_mat_insuf(Black);
}

bool Position::non_pawn_mat(side_t side) const
{
    return minors(side) > 0 || majors(side) > 0;
}

bool Position::non_pawn_mat() const
{
    return non_pawn_mat(White) || non_pawn_mat(Black);
}

int Position::see_max(const Move& m) const
{
    int see = 0;

    if (m.is_capture()) {
        int p6 = P256ToP6[m.capture_piece()];

        assert(piece_is_ok(p6));

        see += ValuePiece[p6][PhaseMg];
    }

    if (m.is_promo()) {
        int p6 = m.promo_piece6();
        
        assert(piece_is_ok(p6));

        see += ValuePiece[p6][PhaseMg] - ValuePawn[PhaseMg];
    }

    return see;
}

int Position::mvv_lva(const Move& m) const
{
    assert(m.is_tactical());

    int mval = 5 - square<6>(m.orig());
    int oval = m.is_capture() ? P256ToP6[m.capture_piece()] + 2 : 0;

    int score = oval * 8 + mval;

    if (m.is_promo()) score += 8;

    return score;
}

void Position::king_zone(bitset<192>& bs, side_t side) const
{
    assert(bs.none());
    
    int ksq = king(side);

         if (ksq == A1) ksq = B2;
    else if (ksq == H1) ksq = G2;
    else if (ksq == A8) ksq = B7;
    else if (ksq == H8) ksq = G7;
    
    bs.set(36 + ksq - 17);
    bs.set(36 + ksq - 16);
    bs.set(36 + ksq - 15);
    bs.set(36 + ksq -  1);
    bs.set(36 + ksq +  1);
    bs.set(36 + ksq + 15);
    bs.set(36 + ksq + 16);
    bs.set(36 + ksq + 17);
}

int Position::lsbishops(side_t side) const
{
    assert(side_is_ok(side));

    int count = 0;

    for (int sq : plist(WB12 + side))
        count += sq88_color(sq) == White;

    return count;
}

int Position::dsbishops(side_t side) const
{
    assert(side_is_ok(side));

    int count = 0;

    for (int sq : plist(WB12 + side))
        count += sq88_color(sq) == Black;

    return count;
}

string Position::to_egtb() const
{
    ostringstream oss;

    oss << 'K';

    oss << string('Q', plist(WQ12).size());
    oss << string('R', plist(WR12).size());
    oss << string('B', plist(WB12).size());
    oss << string('N', plist(WN12).size());
    oss << string('P', plist(WP12).size());

    oss << 'K';
    
    oss << string('Q', plist(BQ12).size());
    oss << string('R', plist(BR12).size());
    oss << string('B', plist(BB12).size());
    oss << string('N', plist(BN12).size());
    oss << string('P', plist(BP12).size());

    return oss.str();
}

bool Position::move_is_recap(const Move& m) const
{
    return pm_.is_capture() && pm_.dest() == m.dest();
}

bool Position::move_is_safe(const Move& m, int& see) const
{
    if (m.is_under()) return false;

    u8 mpiece = square(m.orig());

    if (is_king(mpiece)) return true;

    if (m.is_capture()) {
        u8 opiece = m.capture_piece();
    
        int mp6 = P256ToP6[mpiece];
        int op6 = P256ToP6[opiece];

        if (op6 >= mp6) return true;
    }

    if (see == ScoreNone)
        see = see_move(*this, m);

    return see >= 0;
}

bool Position::move_is_dangerous(const Move& m, bool checks) const
{
    return checkers() || checks || m.is_tactical();
}
