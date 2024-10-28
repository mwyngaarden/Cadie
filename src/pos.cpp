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
#include "attacks.h"
#include "bb.h"
#include "pos.h"
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "search.h"
#include "string.h"
using namespace std;

Position::Position(const string fen, [[maybe_unused]] int score1, [[maybe_unused]] int score2)
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

        ep_sq_ = ep_is_valid(side_, sq) ? sq : square::None;
    }

    // half moves counter

    if (half_moves != "-")
        half_moves_ = stoi(half_moves);

    // full moves counter

    if (full_moves != "-")
        full_moves_ = stoi(full_moves);

    // set pins

    pins_ = get_pins();

    // set checkers

    checkers_ = get_checkers();

    // key

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);

    if (side_ == White)
        key_ ^= zob::side();

#ifdef TUNE
    score1_ = score1;
    score2_ = score2;
#endif
}

#ifdef TUNE
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

#ifndef NDEBUG

    checkers_ = get_checkers();

#endif

    // key

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);

    if (side_ == White)
        key_ ^= zob::side();
}
#endif

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
    oss << (ep_sq_ == square::None ? "-" : square::sq_to_san(ep_sq_));

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

    else if (orig == king()) {
        if (abs(orig - dest) == 2) {
            assert(empty(dest));
            m.set_flag(Move::CastleFlag);
        }
    }

    return m;
}

void Position::make_move_fast(Move m)
{
    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    int orig = m.orig();
    int dest = m.dest();
    int incr = square::incr(side_);

    ep_sq_ = square::None;

    if (m.is_special()) {
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
        else { // m.is_promo()
            if (m.is_capture())
                rem_piece<true>(dest);

            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, m.promo()));
        }
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        mov_piece<true>(orig, dest);
    }

    flags_      &= CastleMasks[orig] & CastleMasks[dest];
    half_moves_  = m.is_irrev() ? 0 : half_moves_ + 1;
    full_moves_ += side_;
    side_       ^= 1;

    pins_ = get_pins();
    checkers_ = get_checkers();

    prev_move_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
}

void Position::make_move(Move m, bool skip_checkers)
{
    assert(m.is_valid());
    assert(m.dest() != king(!side_));
    assert(mstack.empty() || m != mstack.back());

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    int orig = m.orig();
    int dest = m.dest();
    int incr = square::incr(side_);

    ep_sq_ = square::None;

    if (m.is_special()) {
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
        else { // m.is_promo()
            if (m.is_capture())
                rem_piece<true>(dest);

            rem_piece<true>(orig);
            add_piece<true>(dest, to_piece256(side_, m.promo()));
        }
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        mov_piece<true>(orig, dest);
    }

    flags_      &= CastleMasks[orig] & CastleMasks[dest];
    half_moves_  = m.is_irrev() ? 0 : half_moves_ + 1;
    full_moves_ += side_;
    side_       ^= 1;

    pins_ = get_pins();
    checkers_ = skip_checkers ? 0 : get_checkers();

    prev_move_ = m;

    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    
    assert(!side_attacks(side_, king(!side_)));

    kstack.add(key_);
    mstack.add(m);

    si.tnodes++;
}

void Position::unmake_move(const UndoInfo& undo)
{
    Move m = prev_move_;

    assert(m == mstack.back());

    key_        = undo.key;
    pins_       = undo.pins;
    checkers_   = undo.checkers;
    //pawn_key_ = undo.pawn_key;
    flags_      = undo.flags;
    ep_sq_      = undo.ep_sq;
    half_moves_ = undo.half_moves;
    full_moves_ = undo.full_moves;
    prev_move_  = undo.prev_move;

    int orig = m.orig();
    int dest = m.dest();

    if (m.is_special()) {
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
        else if (m.is_double())
            mov_piece<false>(dest, orig);
        else { // m.is_promo()
            if (m.is_promo()) {
                rem_piece<false>(dest);
                add_piece<false>(orig, make_pawn(!side_));
            }

            if (m.is_capture())
                add_piece<false>(dest, m.captured());
        }
    }
    else {
        mov_piece<false>(dest, orig);

        if (m.is_capture())
            add_piece<false>(dest, m.captured());
    }

    side_ ^= 1;
    
    kstack.pop_back();
    mstack.pop_back();
}

bool Position::move_is_sane(Move m)
{
    assert(!m.is_castle());

    int orig = m.orig();
    int dest = m.dest();

    bool insane;

    if (m.is_ep()) {
        rem_piece<false>(dest - square::incr(side_));
        mov_piece<false>(orig, dest);

        insane = side_attacks(!side_, king());

        mov_piece<false>(dest, orig);
        add_piece<false>(square::ep_dual(dest), make_pawn(!side_));
    }
    else {
        if (m.is_capture())
            rem_piece<false>(dest);

        if (m.is_promo()) {
            rem_piece<false>(orig);
            add_piece<false>(dest, to_piece256(side_, m.promo()));
        }
        else
            mov_piece<false>(orig, dest);

        insane = side_attacks(!side_, king());

        if (m.is_promo()) {
            rem_piece<false>(dest);
            add_piece<false>(orig, make_pawn(!side_));
        }
        else
            mov_piece<false>(dest, orig);

        if (m.is_capture())
            add_piece<false>(dest, m.captured());
    }

    return !insane;
}

void Position::make_null()
{
    key_    ^= zob::side();
    key_    ^= zob::ep(ep_sq_);
    side_   ^= 1;

    ep_sq_      = square::None;
    checkers_   = 0;
    prev_move_  = Move::Null();
    pins_       = get_pins();

    kstack.add(key_);
    mstack.add(Move::Null());

    si.tnodes++;
}

void Position::unmake_null(const UndoInfo& undo)
{
    key_        = undo.key;
    pins_       = undo.pins;
    checkers_   = undo.checkers;
    ep_sq_      = undo.ep_sq;
    prev_move_  = undo.prev_move;

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

        //if (is_pawn(piece))
        //    pawn_key_ ^= zob::piece(p12, sq);
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

    assert(piece256_is_ok(piece));

    int p12 = P256ToP12[piece];

    if (UpdateKey) {
        key_ ^= zob::piece(p12, sq);

        //if (is_pawn(piece))
        //    pawn_key_ ^= zob::piece(p12, sq);
    }

    bb_side_[p12 & 1]    ^= bb::bit(sq);
    bb_pieces_[p12 >> 1] ^= bb::bit(sq);

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
        
        //if (is_pawn(piece)) {
        //    pawn_key_ ^= zob::piece(p12, orig);
        //    pawn_key_ ^= zob::piece(p12, dest);
        //}
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

        if ((bb::Between[sq][dest] & occ) == 0)
            return true;
    }

    for (u64 bb = RookAttacks[dest] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::Between[sq][dest] & occ) == 0)
            return true;
    }

    // king

    if (bb::Leorik::King(king(side), bb::bit(dest)))
        return true;

    return false;
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

u64 Position::get_pins() const
{
    u64 def = 0;

    u64 occ = this->occ();
    u64 mbb = bb(side_);
    u64 bbb = bb(!side_, Bishop);
    u64 rbb = bb(!side_, Rook);
    u64 qbb = bb(!side_, Queen);

    int ksq = king();

    for (u64 bb = BishopAttacks[ksq] & (bbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[sq][ksq] & occ;

        if ((between & mbb) && bb::single(between))
            def |= between;
    }

    for (u64 bb = RookAttacks[ksq] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        u64 between = bb::Between[sq][ksq] & occ;

        if ((between & mbb) && bb::single(between))
            def |= between;
    }

    return def;
}

u64 Position::get_checkers() const
{
    u64 att = 0;

    int ksq = king();

    u64 occ = this->occ();
    u64 nbb = bb(!side_, Knight);
    u64 bbb = bb(!side_, Bishop);
    u64 rbb = bb(!side_, Rook);
    u64 qbb = bb(!side_, Queen);

    att |= PawnAttacks[side_][ksq] & bb(!side_, Pawn);
    att |= bb::Leorik::Knight(ksq, nbb);

    for (u64 bb = BishopAttacks[ksq] & (bbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::Between[sq][ksq] & occ) == 0)
            att |= bb::bit(sq);
    }

    for (u64 bb = RookAttacks[ksq] & (rbb | qbb); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::Between[sq][ksq] & occ) == 0)
            att |= bb::bit(sq);
    }

    assert(popcount(att) <= 2);
    assert(!!att == side_attacks(!side_, ksq));

    return att;
}

bool Position::line_is_empty(int orig, int dest) const
{
    assert(sq64_is_ok(orig));
    assert(sq64_is_ok(dest));

    return (bb::Between[orig][dest] & occ()) == 0;
}

bool Position::move_is_check(Move m)
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
        
    if (   type != Knight
        && !m.is_ep() 
        && (!m.is_promo() || m.promo() != Knight)
        && !m.is_castle()
        && ((batt | ratt) & (mobb | mdbb)) == 0)
        return false;

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

                    if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            // Discovered attack by rook/queen?

            else if (ratt & mobb) {
                for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            return false;
        }

        else if (m.is_ep()) {
            int incr = square::incr(side_);

            rem_piece<false>(dest - incr);
            mov_piece<false>(orig, dest);

            bool checks = side_attacks(side_, oksq);

            mov_piece<false>(dest, orig);
            add_piece<false>(dest - incr, make_pawn(!side_));

            return checks;
        }

        else if (m.is_promo()) {
            if (m.is_capture())
                rem_piece<false>(dest);

            rem_piece<false>(orig);
            add_piece<false>(dest, to_piece256(side_, m.promo()));

            bool checks = side_attacks(side_, oksq);

            rem_piece<false>(dest);
            add_piece<false>(orig, make_pawn(side_));

            if (m.is_capture())
                add_piece<false>(dest, m.captured());

            return checks;
        }
        
        else {
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

                    if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            // Discovered attack by rook/queen?

            else if (ratt & mobb) {
                for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
                }
            }

            return false;
        }
    }

    else if (type == Knight) {

        // Direct check?
        if (bb::Leorik::Knight(dest, okbb))
            return true;

        // Discovered check by bishop/queen?
       
        if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }
        
        // Discovered check by rook/queen?
     
        else if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }
       
        return false;
    }

    else if (type == Bishop) {

        // Direct check?
        if (bb::test(batt, dest) && (bb::Between[dest][oksq] & occ) == 0)
            return true;

        // Discovered check by rook/queen?

        if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }

    else if (type == Rook) {

        // Direct check?
        if (bb::test(ratt, dest) && (bb::Between[dest][oksq] & occ) == 0)
            return true;

        // Discovered check not possible by bishop/queen in this case

        if (!square::diag_eq(orig, oksq) && !square::anti_eq(orig, oksq))
            return false;

        if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }

    else if (type == Queen)
        return bb::test(QueenAttacks[oksq], dest) && (bb::Between[dest][oksq] & occ) == 0;

    else {
        assert(type == King);

        if (m.is_castle()) {
            int rdest = dest > orig ? orig + 1 : orig - 1;
            
            if (square::file_eq(rdest, oksq) || square::rank_eq(rdest, oksq))
                return (bb::Between[rdest][oksq] & (occ ^ mobb)) == 0;

            return false;
        }

        // Discovered check not possible if our king is moving on the same line
        // as the opponent king

        if (bb::test(batt, orig) && bb::test(batt, dest)) return false;
        if (bb::test(ratt, orig) && bb::test(ratt, dest)) return false;

        // Discovered check by bishop/queen?

        if (batt & mobb) {
            for (u64 bb = batt & (mbbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        // Discovered check by rook/queen?

        else if (ratt & mobb) {
            for (u64 bb = ratt & (mrbb | mqbb); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][oksq] & (occ ^ mobb)) == 0) return true;
            }
        }

        return false;
    }
}

bool Position::move_is_legal(Move m) const
{
    // Only legal moves are generated when in check
    if (checkers_)
        return true;

    int ksq  = king();
    int orig = m.orig();
    int dest = m.dest();

    if (orig == ksq) {
        if (m.is_castle()) {
            int sq1 = dest > orig ? orig + 1 : orig - 1;
            int sq2 = dest > orig ? orig + 2 : orig - 2;

            return !side_attacks(!side_, sq1) && !side_attacks(!side_, sq2);
        }

        return !side_attacks(!side_, dest);
    }

    if (m.is_ep()) {
        Position tmp = *this;

        return tmp.move_is_sane(m);
    }

    if (!bb::test(pins_, orig))
        return true;

    return abs(delta_incr(ksq, orig)) == abs(delta_incr(orig, dest));
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

bool Position::draw_dead(side_t side) const
{
    assert((bb_pieces_[Pawn] | bb_pieces_[Rook] | bb_pieces_[Queen]) == 0);

    u64 nbb = bb_side_[side] & bb_pieces_[Knight];
    u64 bbb = bb_side_[side] & bb_pieces_[Bishop];

    // bishop and knight
    if (nbb && bbb) return false;

    // opposite colored bishops
    if ((bbb & bb::Light) && (bbb & bb::Dark)) return false;

    // single minor
    if (bb::single(nbb | bbb)) return true;

    return popcount(nbb) <= 2;
}

bool Position::draw_dead() const
{
    if (bb_pieces_[Pawn] | bb_pieces_[Rook] | bb_pieces_[Queen])
        return false;

    return draw_dead(White) && draw_dead(Black);
}

bool Position::draw_fifty() const
{
    if (half_moves_ < 100)
        return false;

    if (!checkers_)
        return true;

    MoveList moves;

    gen_moves(moves, *this, GenMode::Legal);

    return !moves.empty();
}

bool Position::draw_rep() const
{
    for (size_t i = 4; i <= size_t(half_moves_) && i < kstack.size(); i += 2)
        if (kstack.back(i) == kstack.back())
            return true;

    return false;
}

bool Position::draw() const
{
    return draw_rep() || draw_dead() || draw_fifty();
}

int Position::mvv_lva(Move m) const
{
    assert(m.is_tactical());

    int mval = 5 - board<6>(m.orig());
    int oval = m.is_capture() ? P256ToP6[m.captured()] + 2 : 0;

    int score = 8 * oval + mval;

    if (m.is_promo()) score += 8;

    return score;
}

bool Position::move_is_recap(Move m) const
{
    return prev_move_.is_capture() && prev_move_.dest() == m.dest();
}

#ifdef TUNE
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

    return bin;
}
#endif

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

bool Position::see(Move m, int threshold) const
{
    assert(m.is_valid());

    if (m.is_under_promo())
        return 0;

    if (m.is_ep() || m.is_queen_promo() || m.orig() == king())
        return 1;

    int orig = m.orig();
    int dest = m.dest();

    int score = see_max(m) - threshold;

    if (score < 0) return 0;

    score -= SEEValue[board<6>(orig)];

    if (score >= 0) return 1;

#if PROFILE >= PROFILE_SOME
    gstats.stests++;
#endif

    u64 occ = (this->occ() ^ bb::bit(orig)) | bb::bit(dest);
    u64 att = attackers_to(occ, dest);

    u64 bishops = bb_pieces_[Bishop] | bb_pieces_[Queen];
    u64 rooks = bb_pieces_[Rook] | bb_pieces_[Queen];

    side_t side = !side_;

    while (true) {

        att &= occ;

        u64 matt = att & bb_side_[side];

        if (matt == 0) break;

        int type;

        for (type = Pawn; type < King; type++)
            if (matt & bb(side, type))
                break;

        side = !side;

        score = -score - 1 - SEEValue[type];

        if (score >= 0) {
            if (type == King && (att & bb_side_[side]))
                side = !side;

            break;
        }

        occ ^= bb::bit(bb::lsb(matt & bb(side_t(!side), type)));

        if (type == Pawn || type == Bishop || type == Queen)
            att |= bb::Leorik::Bishop(dest, occ) & bishops;

        if (type == Rook || type == Queen)
            att |= bb::Leorik::Rook(dest, occ) & rooks;
    }

    return side != side_;
}

u64 Position::attackers_to(u64 occ, int sq) const
{
    return (PawnAttacks[White][sq] & bb_side_[Black] & bb_pieces_[Pawn])
         | (PawnAttacks[Black][sq] & bb_side_[White] & bb_pieces_[Pawn])
         | bb::Leorik::Knight(sq, bb_pieces_[Knight])
         | (bb::Leorik::Bishop(sq, occ) & (bb_pieces_[Bishop] | bb_pieces_[Queen]))
         | (bb::Leorik::Rook(sq, occ) & (bb_pieces_[Rook] | bb_pieces_[Queen]))
         | bb::Leorik::King(sq, bb_pieces_[King]);
}
