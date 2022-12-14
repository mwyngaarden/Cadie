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
#include "move.h"
#include "piece.h"
#include "search.h"
#include "string.h"
#include "types.h"

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
    key_ ^= zobrist_side(side_);

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
    // from_string handles promotion

    Move move = Move::from_string(s);

    int orig = move.orig();
    int dest = move.dest();

    int delta = orig - dest;

    u8 mpiece = square(orig);
    u8 opiece = square(dest);

    // do this before if pawn, so ep capture will overwrite,
    // and not the other way around

    move.set_capture(opiece);

    if (is_pawn(mpiece)) {
        if (abs(delta) == 32)
            move.set_double();
        else if (dest == ep_sq_) {
            move.set_ep();
            move.set_capture(square(ep_dual(ep_sq_)));
        }
    } 
    else if (is_king(mpiece) && abs(delta) == 2)
        move.set_castle();

    return move;
}

void Position::make_move(const Move& move, MoveUndo& undo)
{
    undo.flags          = flags_;
    undo.ep_sq          = ep_sq_;
    undo.cap_sq         = cap_sq_;
    undo.half_moves     = half_moves_;
    undo.full_moves     = full_moves_;
    undo.key            = key_;
    undo.pawn_key       = pawn_key_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers_count = checkers_count_;

    key_ ^= zobrist_castle(flags_);
    key_ ^= zobrist_ep(ep_sq_);
    key_ ^= zobrist_side(White);

    cap_sq_ = SquareNone;

    int orig  = move.orig();
    int dest  = move.dest();
    int piece = square(orig);
    int pincr = pawn_incr(side_);

    if (move.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (move.is_ep()) {
        assert(dest == ep_sq_);

        rem_piece<true>(dest - pincr);
        mov_piece<true>(orig, dest);
            
        cap_sq_ = dest;
    }
    else {
        if (move.is_capture()) {
            rem_piece<true>(dest);

            cap_sq_ = dest;
        }

        if (move.is_promo()) {
            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, move.promo_piece()));
        } 
        else
            mov_piece<true>(orig, dest);
    }

    flags_      &= CastleLUT[orig] & CastleLUT[dest];
    ep_sq_       = move.is_double() && ep_is_valid(ep_dual(dest)) ? ep_dual(dest) : SquareNone;
    half_moves_  = is_pawn(piece) || move.is_capture() ? 0 : half_moves_ + 1;
    full_moves_ += side_;
    side_        = flip_side(side_);

    // update checkers

    checkers_count_ = 0;

    set_checkers_fast(move);

    key_ ^= zobrist_castle(flags_);
    key_ ^= zobrist_ep(ep_sq_);
    
    assert(!side_attacks(side_, king_sq(flip_side(side_))));

    key_stack.add(key_);
    move_stack.add(move);

    if (Profile >= ProfileLevel::Low) gstats.moves_count++;
}

void Position::unmake_move(const Move& move, const MoveUndo& undo)
{
    flags_          = undo.flags;
    ep_sq_          = undo.ep_sq;
    cap_sq_         = undo.cap_sq;
    half_moves_     = undo.half_moves;
    full_moves_     = undo.full_moves;
    key_            = undo.key;
    pawn_key_       = undo.pawn_key;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_count_ = undo.checkers_count;

    int orig = move.orig();
    int dest = move.dest();

    if (move.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<false>(dest, orig);
        mov_piece<false>(rdest, rorig);
    } 
    else if (move.is_ep()) {
        mov_piece<false>(dest, orig);
        add_piece<false>(ep_dual(dest), make_pawn(side_));
    } 
    else {
        if (move.is_promo()) {
            rem_piece<false>(dest);
            add_piece<false>(orig, make_pawn(flip_side(side_)));
        } 
        else
            mov_piece<false>(dest, orig);

        if (move.is_capture())
            add_piece<false>(dest, move.capture_piece());
    }

    side_ = flip_side(side_);
    
    key_stack.pop_back();
    move_stack.pop_back();
}

void Position::make_null(NullUndo& undo)
{
    undo.key            = key_;
    undo.ep_sq          = ep_sq_;
    undo.cap_sq         = cap_sq_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers_count = checkers_count_;

    key_    ^= zobrist_side(White);
    key_    ^= zobrist_ep(ep_sq_);
    side_    = flip_side(side_);

    ep_sq_  = SquareNone;
    cap_sq_ = SquareNone;

    checkers_sq_[0] = SquareNone;
    checkers_sq_[1] = SquareNone;
    checkers_count_ = 0;

    assert(key() == calc_key());

    key_stack.add(key_);
    move_stack.add(MoveNull);

    if (Profile >= ProfileLevel::Low) gstats.moves_count++;
}

void Position::unmake_null(const NullUndo& undo)
{
    key_            = undo.key;
    ep_sq_          = undo.ep_sq;
    cap_sq_         = undo.cap_sq;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_count_ = undo.checkers_count;

    side_    = flip_side(side_);

    assert(key() == calc_key());

    move_stack.pop_back();
    key_stack.pop_back();
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

    int p12   = P256ToP12[piece];

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

    if (pseudo_attack(king_sq(side), dest, KingFlag256))
        return true;

    {
        u8 pawn256 = make_pawn(side);
        int orig = dest - pawn_incr(side);

        if (square(orig - 1) == pawn256 || square(orig + 1) == pawn256)
            return true;
    }

    for (int orig : plist(WN12 + side))
        if (pseudo_attack(orig, dest, KnightFlag256))
            return true;

    for (const auto& p : P12Flag) {
        for (auto orig : plist_[p.first + side]) {
            if (!pseudo_attack(orig, dest, p.second))
                continue;

            int incr = delta_incr(dest, orig);
            int sq = dest + incr;

            while (square(sq) == PieceNone256)
                sq += incr;

            if (sq == orig)
                return true;
        }
    }

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

string Position::dump() const
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
    key ^= zobrist_side(side_);

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

    int ksq   = king_sq();
    side_t mside = side_;
    side_t oside = flip_side(side_);
    u8 mflag  = make_flag(mside);
    u8 piece;

    for (const auto &p : P12Flag) {
        for (auto orig : plist_[p.first + oside]) {
            if (!pseudo_attack(orig, ksq, p.second))
                continue;

            int incr = delta_incr(ksq, orig);
            int sq1 = ksq + incr;

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
    assert(checkers_count_ == 0);

    int ksq   = king_sq();
    side_t oside = flip_side(side_);
    side_t opawn  = make_pawn(oside);
    int pincr = pawn_incr(oside);

    if (int orig = ksq - pincr - 1; square(orig) == opawn)
        checkers_sq_[checkers_count_++] = orig;
    if (int orig = ksq - pincr + 1; square(orig) == opawn)
        checkers_sq_[checkers_count_++] = orig;

    for (int p12 = WN12 + oside; p12 <= BQ12; p12 += 2) {
        for (int orig : plist_[p12]) {
            if (piece_attacks(orig, ksq)) {
                assert(checkers_count_ < 2);
                checkers_sq_[checkers_count_++] = orig;
            }
        }
    }
}

void Position::set_checkers_fast(const Move& move)
{
    assert(checkers_count_ == 0);

    int ksq  = king_sq();
    int orig = move.orig();
    int dest = move.dest();

    side_t oside = flip_side(side_);

    u8 oflag = make_flag(oside);

    int sq;

    if (move.is_castle()) {
        int rook = dest > orig ? orig + 1 : orig - 1;

        if (pseudo_attack(ksq, rook, RookFlag256)) {
            int incr = delta_incr(ksq, rook);

            sq = ksq + incr;

            while (square(sq) == PieceNone256)
                sq += incr;

            if (sq == rook)
                checkers_sq_[checkers_count_++] = rook;
        }

        return;
    }

    if (move.is_ep()) {
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

            if ((piece & oflag) && pseudo_attack(sq, ksq, piece))
                checkers_sq_[checkers_count_++] = sq;
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

        checkers_sq_[checkers_count_++] = dest;
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

bool Position::move_is_check(const Move& move) const
{
    side_t mside =           side_;
    side_t oside = flip_side(side_);

    u8 mflag  = make_flag(mside);

    int oksq  = king_sq(oside);

    int orig  = move.orig();
    int dest  = move.dest();
            
    u8 piece;

    int sq;

    // promotion
    // ep

    if (move.is_castle()) {
        int rdest = dest > orig ? orig + 1 : orig - 1;

        if (!pseudo_attack(rdest, oksq, RookFlag256))
            return false;

        int inc = delta_incr(rdest, oksq);

        sq = rdest;

        do { sq += inc; } while (square(sq) == PieceNone256);

        return sq == oksq;
    }

    // TODO rewrite without copy of board
    u8 board[16 * 12];
    memcpy(board, square_, sizeof(square_));

    if (move.is_ep()) {
        int epdual = ep_dual(dest);

        board[36 + orig  ] = PieceNone256;
        board[36 + dest  ] = square(orig);
        board[36 + epdual] = PieceNone256;

        if (pseudo_attack(oksq, epdual, QueenFlags256)) {
            int inc = delta_incr(oksq, epdual);

            sq = oksq;

            do { sq += inc; } while ((piece = board[36 + sq]) == PieceNone256);

            if ((piece & mflag) && pseudo_attack(sq, oksq, piece))
                return true;
        }
    }

    else if (move.is_promo()) {
        u8 promo = to_piece256(mside, move.promo_piece());
        
        if (is_knight(promo) && pseudo_attack(dest, oksq, KnightFlag256))
            return true;

        board[36 + orig] = PieceNone256;
        board[36 + dest] = promo;
    }

    else {
        board[36 + dest] = square(orig);
        board[36 + orig] = PieceNone256;
    }

    if (is_knight(board[36 + dest]) && pseudo_attack(dest, oksq, KnightFlag256))
        return true;

    if (pseudo_attack(oksq, orig, QueenFlags256)) {
        int inc = delta_incr(oksq, orig);

        sq = oksq;

        do { sq += inc; } while ((piece = board[36 + sq]) == PieceNone256);

        if ((piece & mflag) && pseudo_attack(sq, oksq, piece))
            return true;
    }

    if (pseudo_attack(oksq, dest, QueenFlags256)) {
        int inc = delta_incr(oksq, dest);

        sq = oksq;

        do { sq += inc; } while ((piece = board[36 + sq]) == PieceNone256);
        
        if ((piece & mflag) && pseudo_attack(sq, oksq, piece))
            return true;
    }

    return false;
}

bool Position::move_is_legal(const Move& move) const
{
    // The move generator only generates legal evasions when in check
    
    if (checkers_count_ > 0)
        return true;

    int ksq  = king_sq();
    int orig = move.orig();
    int dest = move.dest();

    side_t oside = flip_side(side_);
  
    // At move generation we validated that we are not castling over check, but
    // not if we were castling into check. See note in move generation.
    if (orig == ksq)
        return !side_attacks(oside, dest);
    
    if (move.is_ep())
        return move_is_legal_ep(move);

    if (!pseudo_attack(orig, ksq, QueenFlags256))
        return true;

    if (!ray_is_empty(orig, ksq))
        return true;

    int kincr = delta_incr(ksq, orig);
    int mincr = delta_incr(orig, dest);

    if (abs(mincr) == abs(kincr))
        return true;

    u8 oflag = make_flag(oside);
    u8 piece;

    do { orig += kincr; } while ((piece = square(orig)) == PieceNone256);

    return (piece & oflag) == 0 || !pseudo_attack(ksq, orig, piece);
}

// TODO refactor
bool Position::move_is_legal_ep(const Move& move) const
{
    assert(move.is_ep());
    assert(checkers_count_ == 0);

    int ksq = king_sq();

    int orig = move.orig();
    int dest = move.dest();
    side_t mside = side_;
    int pincr = pawn_incr(mside);
    int cap = dest - pincr;
    u8 oflag = make_flag(flip_side(mside));
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
    key_ ^= zobrist_side(White);
    side_ = flip_side(side_);
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

bool operator==(const Position& lhs, const Position& rhs)
{
    for (size_t i = 0; i < 16 * 12; i++)
        if (lhs.square_[i] != rhs.square_[i])
            return false;

    if (lhs.side_ != rhs.side_) return false;

    if (lhs.flags_ != rhs.flags_) return false;

    if (lhs.ep_sq_ != rhs.ep_sq_) return false;
    if (lhs.cap_sq_ != rhs.cap_sq_) return false;

    if (lhs.half_moves_ != rhs.half_moves_) return false;
    if (lhs.full_moves_ != rhs.full_moves_) return false;

    if (lhs.checkers_count_ != rhs.checkers_count_) return false;
    if (lhs.checkers_sq_[0] != rhs.checkers_sq_[0]) return false;
    if (lhs.checkers_sq_[1] != rhs.checkers_sq_[1]) return false;

    if (lhs.key_ != rhs.key_) return false;
    if (lhs.pawn_key_ != rhs.pawn_key_) return false;

    for (size_t i = 0; i < 12; i++) {
        if (lhs.plist_[i].size() != rhs.plist_[i].size())
            return false;

        for (size_t j = 0; j < lhs.plist_[i].size(); j++)
            if (lhs.plist_[i][j] != rhs.plist_[i][j])
                return false;
    }

    return true;
}

int Position::see_est(const Move& move) const
{
    assert(move.is_capture());

    int see = ValuePiece[P256ToP6[square(move.capture_piece())]][PhaseMg];

    if (move.is_promo())
        see -= ValuePiece[move.promo_piece()][PhaseMg] - ValuePiece[Pawn][PhaseMg];
    else
        see -= ValuePiece[P256ToP6[square(move.orig())]][PhaseMg];

    return see;
}

int Position::see_max(const Move& move) const
{
    int score = 0;

    if (move.is_capture()) {
        int ptype = P256ToP6[move.capture_piece()];

        assert(piece_is_ok(ptype));

        score += ValuePiece[ptype][PhaseMg];
    }

    if (move.is_promo()) {
        int ptype = move.promo_piece();
        
        assert(piece_is_ok(ptype));

        score += ValuePiece[ptype][PhaseMg] - ValuePawn[PhaseMg];
    }

    return score;
}

void Position::king_zone(bitset<192>& bs, side_t side) const
{
    assert(bs.none());
    
    int ksq = king_sq(side);

         if (ksq == A1) ksq = B2;
    else if (ksq == H1) ksq = G2;
    else if (ksq == A8) ksq = B7;
    else if (ksq == H8) ksq = G7;
    
    bs.set(36 + ksq - 17);
    bs.set(36 + ksq - 16);
    bs.set(36 + ksq - 15);
    bs.set(36 + ksq -  1);
    bs.set(36 + ksq     ); // ?
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

bool Position::has_bishop_on(side_t side, int color) const
{
    assert(side_is_ok(side));
    assert(side_is_ok(color));

    for (int sq : plist(WB12 + side))
        if (sq88_color(sq) == color)
            return true;

    return false;
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

bool Position::move_is_recap(const Move& move) const
{
    return move.dest() == cap_sq_;
}
