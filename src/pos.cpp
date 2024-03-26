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
#include "attack.h"
#include "bb.h"
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

Position::Position(const string fen, [[maybe_unused]] double outcome)
{
    // init

    for (int i = 0; i < 64; i++)
        board(i) = PieceNone256;

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
            add_piece<true>(to_sq64(file, rank), char_to_piece256(c));
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
        int sq = square::san_to_sq(ep_sq);

        assert(sq64_is_ok(sq));

        ep_sq_ = ep_is_valid(sq) ? sq : SquareNone;
    }

    // half moves counter

    if (half_moves != "-")
        half_moves_ = stoi(half_moves);

    // full moves counter

    if (full_moves != "-")
        full_moves_ = stoi(full_moves);

    // checkers

    set_checkers();

    // key

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);

    if (side_ == White)
        key_ ^= zob::side();

#ifdef TUNE
    outcome_ = outcome;
#endif
}

string Position::to_fen() const
{
    ostringstream oss;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;

        for (int file = 0; file < 8; file++) {
            u8 piece = board(to_sq64(file, rank));

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
    oss << (ep_sq_ == SquareNone ? "-" : square::sq_to_san(ep_sq_));

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

    int orig = m.orig64();
    int dest = m.dest64();

    u8 mpiece = board(orig);
    u8 opiece = board(dest);

    m.set_capture(opiece);

    if (is_pawn(mpiece)) {
        if (abs(orig - dest) == 16) {
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

void Position::make_move(const Move& m)
{
    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    int orig  = m.orig64();
    int dest  = m.dest64();
    int piece = board(orig);
    int pincr = square::incr(side_);

    ep_sq_ = SquareNone;

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (m.is_ep()) {
        rem_piece<true>(dest - pincr);
        mov_piece<true>(orig, dest);
    }
    else if (m.is_double()) {
        mov_piece<true>(orig, dest);

        if (ep_is_valid(square::ep_dual(dest)))
            ep_sq_ = square::ep_dual(dest);
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

    set_checkers();

    move_prev_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
}

void Position::make_move(const Move& m, UndoMove& undo)
{
    assert(m.is_valid());
    assert(m.dest() != king(!side_));
    assert(!is_king(m.capture_piece()));
    assert(mstack.empty() || m != mstack.back());

    undo.flags          = flags_;
    undo.ep_sq          = ep_sq_;
    undo.half_moves     = half_moves_;
    undo.full_moves     = full_moves_;
    undo.key            = key_;
    undo.pawn_key       = pawn_key_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers       = checkers_;
    undo.move_prev      = move_prev_;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    int orig  = m.orig64();
    int dest  = m.dest64();
    int piece = board(orig);
    int pincr = square::incr(side_);

    ep_sq_ = SquareNone;

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (m.is_ep()) {
        rem_piece<true>(dest - pincr);
        mov_piece<true>(orig, dest);
    }
    else if (m.is_double()) {
        mov_piece<true>(orig, dest);

        if (ep_is_valid(square::ep_dual(dest)))
            ep_sq_ = square::ep_dual(dest);
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

    set_checkers();

    move_prev_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    
    assert(!side_attacks(side_, king64(!side_)));

    kstack.add(key_);
    mstack.add(m);

    sinfo.tnodes++;
}

void Position::unmake_move(const Move& m, const UndoMove& undo)
{
    assert(m == mstack.back());

    flags_          = undo.flags;
    ep_sq_          = undo.ep_sq;
    half_moves_     = undo.half_moves;
    full_moves_     = undo.full_moves;
    key_            = undo.key;
    pawn_key_       = undo.pawn_key;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_       = undo.checkers;
    move_prev_      = undo.move_prev;

    int orig = m.orig64();
    int dest = m.dest64();

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<false>(dest, orig);
        mov_piece<false>(rdest, rorig);
    } 
    else if (m.is_ep()) {
        mov_piece<false>(dest, orig);
        add_piece<false>(square::ep_dual(dest), make_pawn(side_));
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
}

bool Position::make_move_hack(const Move& m, bool test)
{
    assert(m.is_valid());
    assert(m.dest() != king(!side_));
    assert(!is_king(m.capture_piece()));

    int orig = m.orig64();
    int dest = m.dest64();

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<false>(orig, dest);
        mov_piece<false>(rorig, rdest);
    }
    else if (m.is_ep()) {
        rem_piece<false>(dest - square::incr(side_));
        mov_piece<false>(orig, dest);
    }
    else if (m.is_double())
        mov_piece<false>(orig, dest);
    else {
        if (m.is_capture())
            rem_piece<false>(dest);

        if (m.is_promo()) {
            rem_piece<false>(orig);
            add_piece<false>(dest, to_piece256(side_, m.promo_piece6()));
        } 
        else
            mov_piece<false>(orig, dest);
    }

    side_ ^= 1;

    bool ret;

    if (test)
        ret = !side_attacks(side_, king64(!side_));
    else {
        checkers_ = 0;

        set_checkers();

        ret = checkers_;
    }
    
    return ret;
}

void Position::unmake_move_hack(const Move& m, u8 csq1, u8 csq2, u8 c)
{
    assert(m == mstack.back());

    checkers_sq_[0] = csq1;
    checkers_sq_[1] = csq2;
    checkers_       = c;

    int orig = m.orig64();
    int dest = m.dest64();

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<false>(dest, orig);
        mov_piece<false>(rdest, rorig);
    } 
    else if (m.is_ep()) {
        mov_piece<false>(dest, orig);
        add_piece<false>(square::ep_dual(dest), make_pawn(side_));
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
}

void Position::make_null(UndoNull& undo)
{
    undo.key            = key_;
    undo.ep_sq          = ep_sq_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers       = checkers_;
    undo.move_prev      = move_prev_;

    key_    ^= zob::side();
    key_    ^= zob::ep(ep_sq_);
    side_   ^= 1;

    ep_sq_          = SquareNone;
    checkers_sq_[0] = SquareNone;
    checkers_sq_[1] = SquareNone;
    checkers_       = 0;
    move_prev_      = MoveNull;

    kstack.add(key_);
    mstack.add(MoveNull);

    sinfo.tnodes++;
}

void Position::unmake_null(const UndoNull& undo)
{
    key_            = undo.key;
    ep_sq_          = undo.ep_sq;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_       = undo.checkers;
    move_prev_      = undo.move_prev;

    side_ ^= 1;

    mstack.pop_back();
    kstack.pop_back();
}

template <bool UpdateKey>
void Position::add_piece(int sq, u8 piece)
{
    assert(sq64_is_ok(sq));
    assert(empty64(sq));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    ++counts_[p12];

    if (UpdateKey) {
        key_ ^= zob::piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zob::piece(p12, sq);
    }

    bb_all_              |= bb::bit(sq);
    bb_side_[p12 & 1]    ^= bb::bit(sq);
    bb_pieces_[p12 >> 1] ^= bb::bit(sq);

    board(sq) = piece;
}

template <bool UpdateKey>
void Position::rem_piece(int sq)
{
    assert(sq64_is_ok(sq));
    assert(!empty64(sq));

    u8 piece = board(sq);

    assert(!is_king(piece));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    --counts_[p12];

    if (UpdateKey) {
        key_ ^= zob::piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zob::piece(p12, sq);
    }

    bb_all_              &= ~bb::bit(sq);
    bb_side_[p12 & 1]    ^=  bb::bit(sq);
    bb_pieces_[p12 >> 1] ^=  bb::bit(sq);

    board(sq) = PieceNone256;
}

template <bool UpdateKey>
void Position::mov_piece(int orig, int dest)
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));
    assert(!empty64(orig));
    assert(empty64(dest));

    u8 piece = board(orig);

    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    if (UpdateKey) {
        key_ ^= zob::piece(p12, orig);
        key_ ^= zob::piece(p12, dest);
        
        if (is_pawn(piece)) {
            pawn_key_ ^= zob::piece(p12, orig);
            pawn_key_ ^= zob::piece(p12, dest);
        }
    }

    bb_all_              ^= bb::bit(orig) | bb::bit(dest);
    bb_side_[p12 & 1]    ^= bb::bit(orig) | bb::bit(dest);
    bb_pieces_[p12 >> 1] ^= bb::bit(orig) | bb::bit(dest);

    swap(board(orig), board(dest));
}

bool Position::side_attacks(side_t side, int dest) const
{
    assert(side_is_ok(side));
    assert(sq64_is_ok(dest));

    // pawns
    
    if (PawnAttacks[!side][dest] & bb(side, Pawn))
        return true;

    // knights and sliders

    for (u64 occ = this->occ(side); occ; ) {
        int sq = bb::pop(occ);

        if (is_pawn(board(sq)) || is_king(board(sq)))
            continue;

        if (piece_attacks(sq, dest))
            return true;
    }

    // king

    if (int ksq = king64(side); pseudo_attack64(ksq, dest, KingFlag256))
        return true;

    return false;
}

bool Position::piece_attacks(int orig, int dest) const
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));
    assert(board(orig) != PieceNone256);

    u8 piece = board(orig);

    assert(piece256_is_ok(piece));

    if (!pseudo_attack64(orig, dest, piece))
        return false;

    return !is_slider(piece) || line_is_empty(orig, dest);
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
            u8 piece = board(to_sq64(file, rank));

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

bool Position::ep_is_valid(int sq) const
{
    assert(sq64_is_ok(sq));
    assert(square::rank(sq) == Rank3 || square::rank(sq) == Rank6);

    sq = square::ep_dual(sq);

    assert(is_pawn(board(sq)));

    u8 opawn = flip_pawn(board(sq));

    int file = square::file(sq);

    return (file != FileA && board(sq - 1) == opawn) || (file != FileH && board(sq + 1) == opawn);
}

void Position::set_checkers()
{
    assert(checkers_ == 0);

    int ksq = king64();

    u8 opawn = make_pawn(!side_);

    int pincr = side_ == White ? -8 : 8;
    int file = square::file(ksq);

    {
        int orig1 = ksq - pincr - 1;
        int orig2 = ksq - pincr + 1;

        if (file != FileA && sq64_is_ok(orig1) && board(orig1) == opawn)
            checkers_sq_[checkers_++] = to_sq88(orig1);

        if (file != FileH && sq64_is_ok(orig2) && board(orig2) == opawn)
            checkers_sq_[checkers_++] = to_sq88(orig2);
    }

    u64 obb = bb(side_^1, Knight)
            | bb(side_^1, Bishop)
            | bb(side_^1, Rook)
            | bb(side_^1, Queen);

    obb &= PieceAttacks[Knight][ksq] | PieceAttacks[Queen][ksq];

    while (obb) {
        int orig64 = bb::pop(obb);
        int orig   = to_sq88(orig64);

        if (piece_attacks(orig64, ksq))
            checkers_sq_[checkers_++] = orig;
    }
}

bool Position::line_is_empty(int orig, int dest) const
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));

    u64 occ = this->occ(White) | this->occ(Black);

    return (InBetween[orig][dest] & occ) == 0;
}

bool Position::move_is_check(const Move& m) const
{
    u8 mflag = make_flag(side_);

    int oksq = king(!side_);
    int orig = m.orig();
    int dest = m.dest();
            
    u8 mpiece = board(to_sq64(orig));
    u8 piece;

    int sq;
    int incr;

#if PROFILE >= PROFILE_SOME
    gstats.ctests++;
#endif

    if (m.is_ep()) {
        Position p = *this;

        return p.make_move_hack(m, false);
    }

    else if (is_pawn(mpiece)) {

        if (PawnAttacks[side_][to_sq64(dest)] & bb::bit88(oksq))
            return true;

        else if (m.is_promo()) {
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
                } while (sq == orig || (sq64_is_ok(to_sq64(sq)) && board(to_sq64(sq)) == PieceNone256));
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

            do { sq += incr; } while (sq64_is_ok(to_sq64(sq)) && board(to_sq64(sq)) == PieceNone256);

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

            do { sq += incr; } while (sq != dest && sq64_is_ok(to_sq64(sq)) && board(to_sq64(sq)) == PieceNone256);

            if (sq == dest)
                return true;
        }
    }

    // Is there a discovered check?

    if (!pseudo_attack(orig, oksq, QueenFlags256))
        return false;

    incr = delta_incr(oksq, orig);
    sq = oksq;

    do {
        sq += incr;

        if (!sq88_is_ok(sq)) return false;
    } while (sq == orig || (piece = board(to_sq64(sq))) == PieceNone256);

    return (piece & mflag) && pseudo_attack(sq, oksq, piece);
}

bool Position::move_is_legal(const Move& m) const
{
    // Only legal evasions are generated when in check
    if (checkers_ > 0) return true;

    int ksq  = king();
    int orig = m.orig();
    int dest = m.dest();

    // At move generation we validated that we are not castling over check, but
    // not if we were castling into check. See note in move generation.
    if (orig == ksq)
        return !side_attacks(!side_, to_sq64(dest));
    
    if (m.is_ep()) {
        Position p = *this;
        
        return p.make_move_hack(m, true);
    }

    if (!pseudo_attack(orig, ksq, QueenFlags256))
        return true;

    if (!line_is_empty(to_sq64(orig), to_sq64(ksq)))
        return true;

    int kincr = delta_incr(ksq, orig);
    int mincr = delta_incr(orig, dest);

    if (abs(mincr) == abs(kincr))
        return true;

    u8 oflag = make_flag(!side_);
    u8 piece;

    do {
        orig += kincr;

        if (!sq88_is_ok(orig)) return true;

    } while ((piece = board(to_sq64(orig))) == PieceNone256);

    return (piece & oflag) == 0 || !pseudo_attack(ksq, orig, piece);
}

int Position::phase() const
{
    int phase = PhaseMax;

    phase -= PhaseN * knights();
    phase -= PhaseB * bishops();
    phase -= PhaseR * rooks();
    phase -= PhaseQ * queens();

    return max(0, phase);
}

int Position::phase_inv(side_t side) const
{
    int phase = 0;

    phase += PhaseN * knights(side);
    phase += PhaseB * bishops(side);
    phase += PhaseR * rooks(side);
    phase += PhaseQ * queens(side);

    return phase;
}

bool Position::draw_mat_insuf(side_t side) const
{
    int p, n, lb, db, r, q;

    counts(p, n, lb, db, r, q, side);
    
    bool ocb = lb && db;

    if (p || (n && (lb || db)) || ocb || r || q) return false;

    if (n > 2) return false;

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

int Position::mvv_lva(const Move& m) const
{
    assert(m.is_tactical());

    int mval = 5 - board<6>(m.orig64());
    int oval = m.is_capture() ? P256ToP6[m.capture_piece()] + 2 : 0;

    int score = oval * 128 + mval * 16;

    if (m.is_promo()) score += 128;

    return score;
}

bool Position::move_is_recap(const Move& m) const
{
    return move_prev_.is_capture() && move_prev_.dest() == m.dest();
}

bool Position::move_is_safe(const Move& m, int& see, int threshold) const
{
    if (m.is_under()) return false;

    u8 mpiece = board(m.orig64());

    if (is_king(mpiece)) return true;

    if (m.is_capture()) {
        u8 opiece = m.capture_piece();
    
        int mtype = P256ToP6[mpiece];
        int otype = P256ToP6[opiece];

        if (otype >= mtype) return true;
    }

    if (see == 2)
        see = see::calc(*this, m, threshold);

    return see;
}

bool Position::move_is_dangerous(const Move& m, bool gives_check) const
{
    return checkers() || gives_check || m.is_tactical();
}
