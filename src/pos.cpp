#include <algorithm>
#include <array>
#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <cassert>
#include <cctype>
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

Position::Position(const string fen, int score1, int score2)
{
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

        ep_sq_ = ep_is_valid(side_, sq) ? sq : square::SquareCount;
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

    // Tuning
    score1_ = score1;
    score2_ = score2;
}

// Tuning
Position::Position(const PositionBin& bin)
{
    size_t pos = 0;

    for (size_t sq = 0; sq < 64; sq++) {
        u8 p12 = bin.get(pos, 4);

        if (piece12_is_ok(p12)) {
            u8 piece = P12ToP256[p12];

            add_piece<true>(sq, piece);
        }

        pos += 4;
    }

    flags_ = bin.get(pos, 8);
    pos += 8;

    side_ = bin.get(pos, 8);
    pos += 8;
    
    ep_sq_ = bin.get(pos, 8);
    pos += 8;
    
    half_moves_ = bin.get(pos, 8);
    pos += 8;
    
    score1_ = i16(bin.get(pos, 16));
    pos += 16;
    
    score2_ = i16(bin.get(pos, 16));
    //pos += 16;

#ifndef NDEBUG

    // checkers

    set_checkers();

#endif

    // key

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);

    if (side_ == White)
        key_ ^= zob::side();
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
    oss << (ep_sq_ == square::SquareCount ? "-" : square::sq_to_san(ep_sq_));

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

    u8 mpiece = board(orig);
    u8 opiece = board(dest);

    m.set_capture(opiece);

    if (is_pawn(mpiece)) {
        if (dest == ep_sq_) {
            assert(empty(dest));
            m.set_flag(Move::EPFlag);
            m.set_capture(make_pawn(!side_));
        }

        else if (!m.is_promo() && !m.is_capture()) {
            if (abs(orig - dest) == 8) {
                assert(empty(dest));
                m.set_flag(Move::SingleFlag);
            }

            else if (abs(orig - dest) == 16) {
                assert(empty(dest));
                m.set_flag(Move::DoubleFlag);
            }
        }
    }

    else if (is_king(mpiece)) {
        if (abs(orig - dest) == 2) {
            assert(empty(dest));
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

    int orig  = m.orig();
    int dest  = m.dest();
    int piece = board(orig);
    int incr = square::incr(side_);

    ep_sq_ = square::SquareCount;

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (m.is_ep()) {
        rem_piece<true>(dest - incr);
        mov_piece<true>(orig, dest);
    }
    else if (m.is_double()) {
        mov_piece<true>(orig, dest);

        if (ep_is_valid(!side_, square::ep_dual(dest)))
            ep_sq_ = square::ep_dual(dest);
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        if (m.is_promo()) {
            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, m.promo6()));
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

    prev_move_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
}

void Position::make_move(const Move& m, UndoMove& undo)
{
    assert(m.is_valid());
    assert(m.dest() != king(!side_));
    assert(!is_king(m.captured()));
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
    undo.prev_move      = prev_move_;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    int orig  = m.orig();
    int dest  = m.dest();
    int piece = board(orig);
    int incr = square::incr(side_);

    ep_sq_ = square::SquareCount;

    if (m.is_castle()) {
        int rorig = dest > orig ? orig + 3 : orig - 4;
        int rdest = dest > orig ? orig + 1 : orig - 1;

        mov_piece<true>(orig, dest);
        mov_piece<true>(rorig, rdest);
    }
    else if (m.is_ep()) {
        rem_piece<true>(dest - incr);
        mov_piece<true>(orig, dest);
    }
    else if (m.is_double()) {
        mov_piece<true>(orig, dest);

        if (ep_is_valid(!side_, square::ep_dual(dest)))
            ep_sq_ = square::ep_dual(dest);
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        if (m.is_promo()) {
            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, m.promo6()));
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

    prev_move_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    
    assert(!side_attacks(side_, king(!side_)));

    kstack.add(key_);
    mstack.add(m);

    si.tnodes++;
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
    prev_move_      = undo.prev_move;

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
            add_piece<false>(dest, m.captured());
    }

    side_ ^= 1;
    
    kstack.pop_back();
    mstack.pop_back();
}

bool Position::make_move_hack(const Move& m)
{
    assert(m.is_valid());
    assert(m.dest() != king(!side_));
    assert(!is_king(m.captured()));

    int orig = m.orig();
    int dest = m.dest();

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
            add_piece<false>(dest, to_piece256(side_, m.promo6()));
        } 
        else
            mov_piece<false>(orig, dest);
    }

    return !side_attacks(!side_, king(side_));
}

void Position::make_null(UndoNull& undo)
{
    undo.key            = key_;
    undo.ep_sq          = ep_sq_;
    undo.checkers_sq[0] = checkers_sq_[0];
    undo.checkers_sq[1] = checkers_sq_[1];
    undo.checkers       = checkers_;
    undo.prev_move      = prev_move_;

    key_    ^= zob::side();
    key_    ^= zob::ep(ep_sq_);
    side_   ^= 1;

    ep_sq_          = square::SquareCount;

    checkers_       = 0;
    prev_move_      = MoveNull;

    kstack.add(key_);
    mstack.add(MoveNull);

    si.tnodes++;
}

void Position::unmake_null(const UndoNull& undo)
{
    key_            = undo.key;
    ep_sq_          = undo.ep_sq;
    checkers_sq_[0] = undo.checkers_sq[0];
    checkers_sq_[1] = undo.checkers_sq[1];
    checkers_       = undo.checkers;
    prev_move_      = undo.prev_move;

    side_ ^= 1;

    mstack.pop_back();
    kstack.pop_back();
}

template <bool UpdateKey>
void Position::add_piece(int sq, u8 piece)
{
    assert(sq64_is_ok(sq));
    assert(empty(sq));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];
    
    if (UpdateKey) {
        key_ ^= zob::piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zob::piece(p12, sq);
    }

    bb_side_[p12 & 1]    ^= bb::bit(sq);
    bb_pieces_[p12 >> 1] ^= bb::bit(sq);

    board(sq) = piece;
}

template <bool UpdateKey>
void Position::rem_piece(int sq)
{
    assert(sq64_is_ok(sq));
    assert(!empty(sq));

    u8 piece = board(sq);

    assert(!is_king(piece));
    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    if (UpdateKey) {
        key_ ^= zob::piece(p12, sq);

        if (is_pawn(piece))
            pawn_key_ ^= zob::piece(p12, sq);
    }

    bb_side_[p12 & 1]    ^=  bb::bit(sq);
    bb_pieces_[p12 >> 1] ^=  bb::bit(sq);

    board(sq) = PieceNone256;
}

template <bool UpdateKey>
void Position::mov_piece(int orig, int dest)
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));
    assert(!empty(orig));
    assert(empty(dest));

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

    // knights

    for (u64 occ = bb(side, Knight); occ; ) {
        int sq = bb::pop(occ);

        if (bb::Leorik::Knight(sq, bb::bit(dest)))
            return true;
    }

    // sliders

    u64 occ = this->occ();
    u64 bbb = bb(side, Bishop);
    u64 rbb = bb(side, Rook);
    u64 qbb = bb(side, Queen);

    for (u64 bb = BishopAttacks[dest] & (bbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::InBetween[sq][dest] & occ) == 0)
            return true;
    }
    
    for (u64 bb = RookAttacks[dest] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::InBetween[sq][dest] & occ) == 0)
            return true;
    }

    // king

    if (bb::Leorik::King(king(side), bb::bit(dest)))
        return true;

    return false;
}

bool Position::piece_attacks(int orig, int dest) const
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));
    assert(!empty(orig));

    u8 piece = board(orig);

    assert(piece256_is_ok(piece));

    if (!pseudo_attack(to_sq88(orig), to_sq88(dest), piece))
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

            if (piece & WhiteFlag256)
                oss << '-' << (char)piece256_to_char(piece) << '-';
            else if (piece & BlackFlag256)
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

bool Position::ep_is_valid(side_t side, int sq) const
{
    assert(sq64_is_ok(sq));
    assert(bb::test(bb(Pawn), square::ep_dual(sq)));

    return PawnAttacks[!side][sq] & bb(side, Pawn);
}

void Position::set_checkers()
{
    assert(checkers_ == 0);

    int ksq = king();

    u64 cbb = 0;

    u64 occ = this->occ();
    u64 nbb = bb(!side_, Knight);
    u64 bbb = bb(!side_, Bishop);
    u64 rbb = bb(!side_, Rook);
    u64 qbb = bb(!side_, Queen);

    cbb |= PawnAttacks[side_][ksq] & bb(!side_, Pawn);
    cbb |= bb::Leorik::Knight(ksq, nbb);

    for (u64 bb = BishopAttacks[ksq] & (bbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::InBetween[sq][ksq] & occ) == 0)
            cbb |= bb::bit(sq);
    }

    for (u64 bb = RookAttacks[ksq] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::InBetween[sq][ksq] & occ) == 0)
            cbb |= bb::bit(sq);
    }

    assert(popcount(cbb) <= 2);

    while (cbb) {
        int sq = bb::pop(cbb);
            
        checkers_sq_[checkers_++] = sq;
    }
}

bool Position::line_is_empty(int orig, int dest) const
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));

    return (bb::InBetween[orig][dest] & occ()) == 0;
}

bool Position::move_is_check(const Move& m)
{
    int oksq  = king(!side_);
    int orig  = m.orig();
    int dest  = m.dest();

    u64 occ   = this->occ();
    u64 okbb  = bb::bit(oksq);
    u64 mobb  = bb::bit(orig);
    u64 mdbb  = bb::bit(dest);
    u64 mbbb  = bb(side_, Bishop);
    u64 mrbb  = bb(side_, Rook);
    u64 mqbb  = bb(side_, Queen);

    u64 batt = BishopAttacks[oksq];
    u64 ratt = RookAttacks[oksq];

    int type = board<6>(orig);
    
    if (m.is_ep()) [[unlikely]] {
        int incr = square::incr(side_);

        rem_piece<false>(dest - incr);
        mov_piece<false>(orig, dest);
    
        bool checks = side_attacks(side_, oksq);
        
        mov_piece<false>(dest, orig);
        add_piece<false>(dest - incr, make_pawn(!side_));

        return checks;
    }
    
    else if (m.is_castle()) [[unlikely]] {
        int rdest = dest > orig ? orig + 1 : orig - 1;
        
        if (square::file_eq(rdest, oksq) || square::rank_eq(rdest, oksq))
            return (bb::InBetween[rdest][oksq] & (occ ^ mobb)) == 0;

        return false;
    }

    else if (type == Knight) {

        // Direct check?
        if (bb::Leorik::Knight(dest, okbb))
            return true;

        // Discovered check by bishop/queen?
       
        if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }
        
        // Discovered check by rook/queen?
     
        else if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }
       
        return false;
    }
    
    else if (((batt | ratt) & (mobb | mdbb)) == 0)
        return false;
    
    else if (type == Bishop) {

        // Direct check?
        if (bb::test(batt, dest) && (bb::InBetween[dest][oksq] & occ) == 0)
            return true;
        
        // Discovered check by rook/queen?

        if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }
    
    else if (type == Rook) {

        // Direct check?
        if (bb::test(ratt, dest) && (bb::InBetween[dest][oksq] & occ) == 0)
            return true;

        // Discovered check not possible by bishop/queen in this case
       
        if (!square::diag_eq(orig, oksq) && !square::anti_eq(orig, oksq))
            return false;

        if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }

    else if (type == Queen)
        return bb::test(QueenAttacks[oksq], dest) && (bb::InBetween[dest][oksq] & occ) == 0;

    else if (type == King) {

        // Discovered check not possible if our king is moving on the same line
        // as the opponent king

        if (bb::test(batt, orig) && bb::test(batt, dest)) return false;
        if (bb::test(ratt, orig) && bb::test(ratt, dest)) return false;

        // Discovered check by bishop/queen?
        
        if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }
        
        // Discovered check by rook/queen?

        else if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }
   
    else if (type == Pawn) {

        if (m.is_single() || m.is_double()) {

            // Direct check?
            if (PawnAttacks[side_][dest] & okbb)
                return true;

            // Discovered check not possible in these cases
            
            if (square::file_eq(orig, oksq))
                return false;

            if ((bb::FileA | bb::FileH) & mobb)
                return false;

            // Discovered check by bishop/queen?

            if (batt & mobb) {
                for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }
            
            // Discovered attack by rook/queen?

            else if (ratt & mobb) {
                for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            return false;
        }

        else if (!m.is_promo()) {
            assert(m.is_capture());

            // Direct check?
            if (PawnAttacks[side_][dest] & okbb)
                return true;
            
            // Discovered check not possible in these cases

            if (bb::test(batt, orig) && bb::test(batt, dest))
                return false;
        
            // Discovered check by bishop/queen?

            if (batt & mobb) {
                for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            // Discovered attack by rook/queen?

            else if (ratt & mobb) {
                for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::InBetween[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            return false;
        }
    }

    // Pawn promotions handled here

    assert(m.is_promo());
    
    u8 piece = m.captured();

    if (m.is_capture())
        rem_piece<false>(dest);

    rem_piece<false>(orig);
    add_piece<false>(dest, to_piece256(side_, m.promo6()));

    bool checks = side_attacks(side_, oksq);
    
    rem_piece<false>(dest);
    add_piece<false>(orig, make_pawn(side_));
    
    if (m.is_capture())
        add_piece<false>(dest, piece);

    return checks;
}

bool Position::move_is_legal(const Move& m, u64 pins) const
{
    // Only legal evasions are generated when in check
    if (checkers_ > 0) [[unlikely]]
        return true;

    int ksq  = king();
    int orig = m.orig();
    int dest = m.dest();

    // At move generation we validated that we are not castling over check, but
    // not if we were castling into check. See note in move generation.
    if (orig == ksq) [[unlikely]]
        return !side_attacks(!side_, dest);
    
    if (m.is_ep()) [[unlikely]] {
        Position p = *this;
        
        return p.make_move_hack(m);
    }

    if (!bb::test(pins, orig)) [[likely]]
        return true;

    if (square::file_eq(orig, ksq) && square::file_eq(dest, ksq)) return true;
    if (square::rank_eq(orig, ksq) && square::rank_eq(dest, ksq)) return true;
    if (square::diag_eq(orig, ksq) && square::diag_eq(dest, ksq)) return true;
    if (square::anti_eq(orig, ksq) && square::anti_eq(dest, ksq)) return true;

    return false;
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

int Position::phase(side_t side) const
{
    int phase = 0;

    phase += PhaseN * knights(side);
    phase += PhaseB * bishops(side);
    phase += PhaseR * rooks(side);
    phase += PhaseQ * queens(side);

    return min(phase, PhaseMax);
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

int Position::mvv_lva(const Move& m) const
{
    assert(m.is_tactical());

    int mval = 5 - board<6>(m.orig());
    int oval = m.is_capture() ? P256ToP6[m.captured()] + 2 : 0;

    int score = 8 * oval + mval;

    if (m.is_promo()) score += 8;

    return score;
}

bool Position::move_is_recap(const Move& m) const
{
    return prev_move_.is_capture() && prev_move_.dest() == m.dest();
}

bool Position::move_is_safe(const Move& m, int& see, int threshold) const
{
    if (see == 2)
        see = see::calc(*this, m, threshold);

    return see;
}

// Tuning
PositionBin Position::serialize() const
{
    PositionBin bin;

    size_t pos = 0;

    for (size_t sq = 0; sq < 64; sq++) {
        u8 piece = board(sq);
        u8 p12 = P256ToP12[piece];

        bin.set(pos, p12);
        pos += 4;
    }

    bin.set(pos, flags_);
    pos += 8;

    bin.set(pos, side_);
    pos += 8;

    bin.set(pos, ep_sq_);
    pos += 8;
   
    bin.set(pos, half_moves_);
    pos += 8;
    
    bin.set(pos, u16(score1_));
    pos += 16;
    
    bin.set(pos, u16(score2_));
    //pos += 16;

    return bin;
}

string Position::to_egtb() const
{
    ostringstream oss;

    oss << 'K';
    oss << string(queens(White),  'Q');
    oss << string(rooks(White),   'R');
    oss << string(bishops(White), 'B');
    oss << string(knights(White), 'N');
    oss << string(pawns(White),   'P');
    oss << 'K';
    oss << string(queens(Black),  'Q');
    oss << string(rooks(Black),   'R');
    oss << string(bishops(Black), 'B');
    oss << string(knights(Black), 'N');
    oss << string(pawns(Black),   'P');

    return oss.str();
}
