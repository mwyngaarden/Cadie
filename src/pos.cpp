#include <algorithm>
#include <array>
#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
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

Position::Position(const string fen)
{
    for (size_t i = 0; i < 64; i++) square_[i] = None12;

    // fen

    Tokenizer fields(fen);

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
            add_piece<true>(to_sq(file, rank), char_to_piece12(c));
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

        ep_sq_ = ep_is_valid(side_, sq) ? sq : square::None;
    }

    // half moves counter

    if (half_moves != "-")
        half_moves_ = stoi(half_moves);

    // full moves counter

    if (full_moves != "-")
        full_moves_ = stoi(full_moves);

    // set pins and checkers

    set_pins_checkers();

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
            Piece12 pt12 = square(to_sq(file, rank));

            if (pt12 == None12)
                empty++;
            else {
                if (empty > 0) {
                    oss << to_string(empty);
                    empty = 0;
                }

                oss << piece12_to_char(pt12);
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

    Piece12 pt12 = square(orig);
    Piece12 xpt12 = square(dest);

    m.set_capture(xpt12);

    if (pt12 == WP12 || pt12 == BP12) {
        if (dest == ep_sq_) {
            m.set_flag(Move::EPFlag);
            m.set_capture(Piece12(WP12 + !side_));
        }

        else if (!m.is_promo() && !m.is_capture()) {
            if (abs(orig - dest) == 8)
                m.set_flag(Move::SingleFlag);

            else if (abs(orig - dest) == 16)
                m.set_flag(Move::DoubleFlag);
        }
    }

    else if (orig == king()) {
        if (abs(orig - dest) == 2)
            m.set_flag(Move::CastleFlag);
    }

    return m;
}

void Position::make_move(Move m)
{
    key_ ^= zob::castle(flags_);
    key_ ^= zob::ep(ep_sq_);
    key_ ^= zob::side();

    Side sd = side_;
    Side xd = !side_;

    int orig = m.orig();
    int dest = m.dest();
    int incr = square::incr(sd);

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

            if (ep_is_valid(xd, square::ep_dual(dest)))
                ep_sq_ = square::ep_dual(dest);
        }
        else { // m.is_promo()
            if (m.is_capture())
                rem_piece<true>(dest);

            rem_piece<true>(orig);
            add_piece<true>(dest, m.promo(sd));
        }
    }
    else {
        if (m.is_capture())
            rem_piece<true>(dest);

        mov_piece<true>(orig, dest);
    }

    flags_      &= CastleMasks[orig] & CastleMasks[dest];
    half_moves_  = m.is_irrev() ? 0 : half_moves_ + 1;
    full_moves_ += sd;

    side_ = xd;

    set_pins_checkers();

    prev_move_   = m;
    key_        ^= zob::castle(flags_);
    key_        ^= zob::ep(ep_sq_);
    
    kstack.add(key_);
    mstack.add(m);

    si.tnodes++;
}

void Position::unmake_move(const UndoInfo& undo)
{
    Side sd = side_;
    Side xd = !side_;

    Move m = prev_move_;

    key_        = undo.key;
    pinned_     = undo.pinned;
    pinners_    = undo.pinners;
    checkers_   = undo.checkers;
    //pawn_key_   = undo.pawn_key;
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
            add_piece<false>(square::ep_dual(dest), WP12 + sd);
        }
        else if (m.is_double())
            mov_piece<false>(dest, orig);
        else { // m.is_promo()
            if (m.is_promo()) {
                rem_piece<false>(dest);
                add_piece<false>(orig, WP12 + xd);
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

    side_ = xd;
    
    kstack.pop_back();
    mstack.pop_back();
}

bool Position::move_is_sane(Move m)
{
    Side sd = side_;
    Side xd = !side_;

    int orig = m.orig();
    int dest = m.dest();

    bool insane;

    if (m.is_ep()) {
        rem_piece<false>(dest - square::incr(sd));
        mov_piece<false>(orig, dest);

        insane = side_attacks(xd, king());

        mov_piece<false>(dest, orig);
        add_piece<false>(square::ep_dual(dest), WP12 + xd);
    }
    else {
        if (m.is_capture())
            rem_piece<false>(dest);

        if (m.is_promo()) {
            rem_piece<false>(orig);
            add_piece<false>(dest, m.promo(sd));
        }
        else
            mov_piece<false>(orig, dest);

        insane = side_attacks(xd, king());

        if (m.is_promo()) {
            rem_piece<false>(dest);
            add_piece<false>(orig, WP12 + xd);
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
    side_    = !side_;

    ep_sq_      = square::None;
    prev_move_  = Move::Null();

    kstack.add(key_);
    mstack.add(Move::Null());

    si.tnodes++;
}

void Position::unmake_null(const UndoInfo& undo)
{
    key_        = undo.key;
    pinned_     = undo.pinned;
    pinners_    = undo.pinners;
    checkers_   = undo.checkers;
    ep_sq_      = undo.ep_sq;
    prev_move_  = undo.prev_move;

    side_ = !side_;

    mstack.pop_back();
    kstack.pop_back();
}

template <bool UpdateKey>
void Position::add_piece(int sq, Piece12 pt12)
{
    if (UpdateKey)
        key_ ^= zob::piece(pt12, sq);

    Side sd = pt12 % 2;
    Piece pt = pt12 / 2;
    u64 flip = bb::bit(sq);

    if (pt == King) king_[sd] = sq;

    count_[pt12]++;
    bb_side_[sd]  ^= flip;
    bb_piece_[pt] ^= flip;
    square_[sq]    = pt12;
}

template <bool UpdateKey>
void Position::rem_piece(int sq)
{
    Piece12 pt12 = square_[sq];

    if (UpdateKey)
        key_ ^= zob::piece(pt12, sq);

    Side sd = pt12 % 2;
    Piece pt = pt12 / 2;
    u64 flip = bb::bit(sq);

    count_[pt12]--;
    bb_side_[sd]  ^= flip;
    bb_piece_[pt] ^= flip;
    square_[sq]    = None12;
}

template <bool UpdateKey>
void Position::mov_piece(int orig, int dest)
{
    Piece12 pt12 = square_[orig];

    if (UpdateKey)
        key_ ^= zob::piece(pt12, orig) ^ zob::piece(pt12, dest);

    Side sd = pt12 % 2;
    Piece pt = pt12 / 2;
    u64 flip = bb::bit(orig, dest);

    if (pt == King) king_[sd] = dest;

    bb_side_[sd]  ^= flip;
    bb_piece_[pt] ^= flip;

    swap(square_[orig], square_[dest]);
}

bool Position::side_attacks(Side sd, int dest) const
{
    u64 occ     = this->occ();
    u64 knights = bb(sd, Knight);
    u64 bishops = bb(sd, Bishop);
    u64 rooks   = bb(sd, Rook);
    u64 queens  = bb(sd, Queen);

    // pawns
    
    if (PawnAttacks[!sd][dest] & bb(sd, Pawn))
        return true;

    // knights

    if (bb::test(bb::KnightAttacks(knights), dest))
        return true;

    // sliders

    for (u64 bb = BishopAttacks[dest] & (bishops | queens); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::Between[sq][dest] & occ) == 0)
            return true;
    }

    for (u64 bb = RookAttacks[dest] & (rooks | queens); bb; ) {
        int sq = bb::pop(bb);

        if ((bb::Between[sq][dest] & occ) == 0)
            return true;
    }

    // king

    return bb::test(KingAttacks[king(sd)], dest);
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
            Piece12 pt12 = square(to_sq(file, rank));

            if (pt12 == None12) {
                int dark = ~(file ^ rank) & 1;
                oss << ' ' << (dark ? '.' : ' ') << ' ';
            }
            else if (pt12 & 1)
                oss << '<' << (char)toupper(piece12_to_char(pt12)) << '>';
            else
                oss << '-' << (char)piece12_to_char(pt12) << '-';

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

bool Position::ep_is_valid(Side sd, int sq) const
{
    return PawnAttacks[!sd][sq] & bb(sd, Pawn);
}

void Position::set_pins_checkers()
{
    pinned_   = 0;
    pinners_  = 0;
    checkers_ = 0;

    u64 occ = this->occ();

    {
        Side sd = side_;
        Side xd = !side_;

        checkers_ |= PawnAttacks[sd][king(sd)] & bb(xd, Pawn);
        checkers_ |= KnightAttacks[king(sd)] & bb(xd, Knight);
    }

    for (Side sd : { White, Black }) {
        Side xd = !sd;

        u64 socc    = bb(sd);
        u64 bishops = bb(xd, Bishop);
        u64 rooks   = bb(xd, Rook);
        u64 queens  = bb(xd, Queen);

        int king = this->king(sd);

        for (u64 bb = BishopAttacks[king] & (bishops | queens); bb; ) {
            int sq = bb::pop(bb);

            u64 between = bb::Between[sq][king] & occ;

            if ((between & socc) && bb::single(between)) {
                pinned_  |= between;
                pinners_ |= bb::bit(sq);
            }
            else if (between == 0)
                checkers_ |= bb::bit(sq);
        }

        for (u64 bb = RookAttacks[king] & (rooks | queens); bb; ) {
            int sq = bb::pop(bb);

            u64 between = bb::Between[sq][king] & occ;

            if ((between & socc) && bb::single(between)) {
                pinned_  |= between;
                pinners_ |= bb::bit(sq);
            }
            else if (between == 0)
                checkers_ |= bb::bit(sq);
        }
    }
}

bool Position::move_is_check(Move m)
{
    Side sd = side_;
    Side xd = !side_;

    int king = this->king(xd);
    int orig = m.orig();
    int dest = m.dest();

    u64 occ     = this->occ();
    u64 bishops = bb(sd, Bishop);
    u64 rooks   = bb(sd, Rook);
    u64 queens  = bb(sd, Queen);

    u64 kingbit = bb::bit(king);
    u64 origbit = bb::bit(orig);
    u64 destbit = bb::bit(dest);

    u64 batt = BishopAttacks[king];
    u64 ratt = RookAttacks[king];

    int type = square(orig) / 2;

    if (   type != Knight
        && !m.is_ep() 
        && (!m.is_promo() || m.promo() != Knight)
        && !m.is_castle()
        && ((batt | ratt) & (origbit | destbit)) == 0)
        return false;

    else if (type == Pawn) {
        if (m.is_single() || m.is_double()) {

            // Direct check?
            if (PawnAttacks[sd][dest] & kingbit)
                return true;

            // Discovered check not possible in these cases

            if (square::file_eq(orig, king))
                return false;

            if ((bb::FileA | bb::FileH) & origbit)
                return false;

            // Discovered check by bishop/queen?

            if (batt & origbit) {
                for (u64 bb = batt & (bishops | queens); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
                }
            }

            // Discovered attack by rook/queen?

            else if (ratt & origbit) {
                for (u64 bb = ratt & (rooks | queens); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
                }
            }

            return false;
        }

        else if (m.is_ep()) {
            int incr = square::incr(sd);

            rem_piece<false>(dest - incr);
            mov_piece<false>(orig, dest);

            bool checks = side_attacks(sd, king);

            mov_piece<false>(dest, orig);
            add_piece<false>(dest - incr, WP12 + xd);

            return checks;
        }

        else if (m.is_promo()) {
            if (m.is_capture())
                rem_piece<false>(dest);

            rem_piece<false>(orig);
            add_piece<false>(dest, m.promo(sd));

            bool checks = side_attacks(sd, king);

            rem_piece<false>(dest);
            add_piece<false>(orig, WP12 + sd);

            if (m.is_capture())
                add_piece<false>(dest, m.captured());

            return checks;
        }
        
        else {
            // Direct check?
            if (PawnAttacks[sd][dest] & kingbit)
                return true;

            // Discovered check not possible in these cases

            if ((batt & origbit) && (batt & destbit))
                return false;

            // Discovered check by bishop/queen?

            if (batt & origbit) {
                for (u64 bb = batt & (bishops | queens); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
                }
            }

            // Discovered attack by rook/queen?

            else if (ratt & origbit) {
                for (u64 bb = ratt & (rooks | queens); bb; ) {
                    int sq = bb::pop(bb);

                    if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
                }
            }

            return false;
        }
    }

    else if (type == Knight) {

        // Direct check?
        if (KnightAttacks[dest] & kingbit)
            return true;

        // Discovered check by bishop/queen?
       
        if (batt & origbit) {
            for (u64 bb = batt & (bishops | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
            }
        }
        
        // Discovered check by rook/queen?
     
        else if (ratt & origbit) {
            for (u64 bb = ratt & (rooks | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
            }
        }
       
        return false;
    }

    else if (type == Bishop) {

        // Direct check?
        if ((batt & destbit) && (bb::Between[dest][king] & occ) == 0)
            return true;

        // Discovered check by rook/queen?

        if (ratt & origbit) {
            for (u64 bb = ratt & (rooks | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
            }
        }

        return false;
    }

    else if (type == Rook) {

        // Direct check?
        if ((ratt & destbit) && (bb::Between[dest][king] & occ) == 0)
            return true;

        // Discovered check not possible by bishop/queen in this case

        if (!square::diag_eq(orig, king) && !square::anti_eq(orig, king))
            return false;

        if (batt & origbit) {
            for (u64 bb = batt & (bishops | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
            }
        }

        return false;
    }

    else if (type == Queen)
        return (QueenAttacks[king] & destbit) && (bb::Between[dest][king] & occ) == 0;

    else {
        if (m.is_castle()) {
            int rdest = dest > orig ? orig + 1 : orig - 1;
            
            if (square::file_eq(rdest, king) || square::rank_eq(rdest, king))
                return (bb::Between[rdest][king] & (occ ^ origbit)) == 0;

            return false;
        }

        // Discovered check not possible if our king is moving on the same line
        // as the opponent king

        if ((batt & origbit) && (batt & destbit)) return false;
        if ((ratt & origbit) && (ratt & destbit)) return false;

        // Discovered check by bishop/queen?

        if (batt & origbit) {
            for (u64 bb = batt & (bishops | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
            }
        }

        // Discovered check by rook/queen?

        else if (ratt & origbit) {
            for (u64 bb = ratt & (rooks | queens); bb; ) {
                int sq = bb::pop(bb);

                if ((bb::Between[sq][king] & (occ ^ origbit)) == 0) return true;
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

    int king = this->king();
    int orig = m.orig();
    int dest = m.dest();

    if (orig == king) {
        Side xd = !side_;

        if (m.is_castle()) {
            int sq1 = dest > orig ? orig + 1 : orig - 1;
            int sq2 = dest > orig ? orig + 2 : orig - 2;

            return !side_attacks(xd, sq1) && !side_attacks(xd, sq2);
        }

        return !side_attacks(xd, dest);
    }

    if (m.is_ep()) {
        Position tmp = *this;

        return tmp.move_is_sane(m);
    }

    if (!bb::test(pinned_, orig))
        return true;

    return bb::test(bb::Line[king][orig], dest);
}

int Position::phase() const
{
    auto [n, b, r, q] = counts();

    int phase = 24;

    phase -= 1 * n;
    phase -= 1 * b;
    phase -= 2 * r;
    phase -= 4 * q;

    return max(0, phase);
}

int Position::phase(Side sd) const
{
    auto [n, b, r, q] = counts(sd);

    int phase = 0;

    phase += 1 * n;
    phase += 1 * b;
    phase += 2 * r;
    phase += 4 * q;

    return min(phase, 24);
}

bool Position::draw_dead(Side sd) const
{
    u64 knights = bb(sd, Knight);
    u64 bishops = bb(sd, Bishop);

    // bishop and knight
    if (knights && bishops) return false;

    // opposite colored bishops
    if ((bishops & bb::Light) && (bishops & bb::Dark)) return false;

    // single minor
    if (bb::single(knights | bishops)) return true;

    return count_[WN12 + sd] <= 2;
}

bool Position::draw_dead() const
{
    if (bb(Pawn, Rook, Queen)) return false;

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
    int mval = 5 - square(m.orig()) / 2;
    int oval = m.is_capture() ? m.cap6() + 2 : 0;

    int score = 8 * oval + mval;

    if (m.is_promo()) score += 8;

    return score;
}

bool Position::move_is_recap(Move m) const
{
    Move pm = prev_move_;

    return pm.is_valid() && pm.is_capture() && pm.dest() == m.dest();
}

bool Position::see(Move m, int threshold) const
{
    if (m.is_under_promo())
        return 0;

    if (m.is_ep() || m.is_queen_promo() || m.orig() == king())
        return 1;

    int orig = m.orig();
    int dest = m.dest();

    int score = SEEValue[m.cap9()] - threshold;

    if (score < 0) return 0;

    score = SEEValue[square(orig) / 2] - score;

    if (score <= 0) return 1;

    u64 occ = this->occ() ^ bb::bit(orig, dest);
    u64 att = attackers_to(occ, dest);

    u64 bishops = bb(Bishop, Queen);
    u64 rooks   = bb(Rook, Queen);

    int ret = 1;

    u64 minatt;

    for (Side sd = !side_; ; sd = !sd) {
        att &= occ;

        u64 satt = att & bb_side_[sd];

        if (!satt) break;

        ret ^= 1;

        if (minatt = satt & bb(sd, Pawn); minatt) {
            score = SEEValue[Pawn] - score;

            if (score < ret) break;

            occ ^= minatt & -minatt;
            att |= bb::Leorik::Bishop(dest, occ) & bishops;
        }

        else if (minatt = satt & bb(sd, Knight); minatt) {
            score = SEEValue[Knight] - score;

            if (score < ret) break;

            occ ^= minatt & -minatt;
        }

        else if (minatt = satt & bb(sd, Bishop); minatt) {
            score = SEEValue[Bishop] - score;

            if (score < ret) break;

            occ ^= minatt & -minatt;
            att |= bb::Leorik::Bishop(dest, occ) & bishops;
        }

        else if (minatt = satt & bb(sd, Rook); minatt) {
            score = SEEValue[Rook] - score;

            if (score < ret) break;

            occ ^= minatt & -minatt;
            att |= bb::Leorik::Rook(dest, occ) & rooks;
        }

        else if (minatt = satt & bb(sd, Queen); minatt) {
            score = SEEValue[Queen] - score;

            if (score < ret) break;

            occ ^= minatt & -minatt;
            att |= (bb::Leorik::Bishop(dest, occ) & bishops) | (bb::Leorik::Rook(dest, occ) & rooks);
        }
        else
            return att & ~bb_side_[sd] ? ret ^ 1 : ret;
    }

    return ret;
}

u64 Position::attackers_to(u64 occ, int sq) const
{
    return (PawnAttacks[White][sq] & bb(Black, Pawn))
         | (PawnAttacks[Black][sq] & bb(White, Pawn))
         | (KnightAttacks[sq] & bb(Knight))
         | (bb::Leorik::Bishop(sq, occ) & bb(Bishop, Queen))
         | (bb::Leorik::Rook(sq, occ) & bb(Rook, Queen))
         | (KingAttacks[sq] & bb(King));
}

u64 Position::static_attacks(Side sd) const
{
    u64 pawns   = bb(sd, Pawn);
    u64 knights = bb(sd, Knight);

    int king = this->king(sd);

    return bb::PawnAttacks(sd, pawns) | bb::KnightAttacks(knights) | KingAttacks[king];
}
