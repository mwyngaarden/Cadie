#ifndef POS_H
#define POS_H

#include <bit>
#include <bitset>
#include <string>
#include <tuple>
#include <vector>
#include <cstdint>
#include <cstring>
#include "bb.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "square.h"
#include "zobrist.h"


class Position {
public:
    static constexpr char StartPos[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    static constexpr int WhiteCastleKFlag = 1 << 0;
    static constexpr int BlackCastleKFlag = 1 << 1;
    static constexpr int WhiteCastleQFlag = 1 << 2;
    static constexpr int BlackCastleQFlag = 1 << 3;
    static constexpr int WhiteCastleFlags = WhiteCastleQFlag | WhiteCastleKFlag;
    static constexpr int BlackCastleFlags = BlackCastleQFlag | BlackCastleKFlag;
    static constexpr int CastleFlags = BlackCastleQFlag | WhiteCastleQFlag | BlackCastleKFlag | WhiteCastleKFlag;

    Position() = default;
    Position(const std::string fen);
    Position(const char fen[]) : Position(std::string(fen)) { }

    Move note_move(const std::string& s) const;

    void   make_move(Move m);
    void unmake_move(const UndoInfo& undo);

    void   make_null();
    void unmake_null(const UndoInfo& undo);

    UndoInfo undo_info() const
    {
        UndoInfo undo;

        undo.key            = key_;
        undo.pinned         = pinned_;
        undo.pinners        = pinners_;
        undo.checkers       = checkers_;
        undo.flags          = flags_;
        undo.ep_sq          = ep_sq_;
        undo.half_moves     = half_moves_;
        undo.full_moves     = full_moves_;
        undo.prev_move      = prev_move_;

        return undo;
    }

    std::string to_fen() const;

    u64 key() const { return key_; }

    bool move_is_check  (Move m);
    bool move_is_recap  (Move m) const;
    bool move_is_legal  (Move m) const;
    bool move_is_sane   (Move m);

    int mvv_lva         (Move m) const;

    bool ep_is_valid    (Side sd, int sq) const;
    bool side_attacks   (Side sd, int dest) const;

    Piece12 square(int sq) const
    {
        return square_[sq];
    }

    bool empty(int sq) const
    {
        return square_[sq] == None12;
    }

    int king(Side sd) const { return king_[sd]; }
    int king(       ) const { return king_[side_]; }

    Side side() const { return side_; }

    bool can_castle_k(Side sd) const { return flags_ & (WhiteCastleKFlag << sd); }
    bool can_castle_q(Side sd) const { return flags_ & (WhiteCastleQFlag << sd); }
    bool can_castle  (Side sd) const { return flags_ & (WhiteCastleFlags << sd); }

    bool can_castle_k()        const { return can_castle_k(side_); }
    bool can_castle_q()        const { return can_castle_q(side_); }
    bool can_castle  ()        const { return can_castle  (side_); }

    u8 flags() const { return flags_; }

    u8 ep_sq() const { return ep_sq_; }
    u8 ep_sq(u8 sq) { return ep_sq_ = sq; }

    u8  half_moves() const { return half_moves_; }
    u16 full_moves() const { return full_moves_; }

    bool draw_dead  (Side sd) const;
    bool draw_dead  () const;
    bool draw_fifty () const;
    bool draw_rep   () const;
    bool draw       () const;

    int draw_scale(int eval) const { return (200 - half_moves_) * eval / 200; }

    int count() const { return bb::count(occ()); }

    template<Side SD>
    auto counts() const
    {
        constexpr Side XD = !SD;

        return std::make_tuple(count_[WN12 + SD],
                               count_[WB12 + SD],
                               count_[WR12 + SD],
                               count_[WQ12 + SD],
                               count_[WN12 + XD],
                               count_[WB12 + XD],
                               count_[WR12 + XD],
                               count_[WQ12 + XD]);
    }

    auto counts(Side sd) const
    {
        return std::make_tuple(count_[WN12 + sd], count_[WB12 + sd], count_[WR12 + sd], count_[WQ12 + sd]);
    }

    auto counts() const
    {
        return std::make_tuple(count_[WN12] + count_[BN12],
                               count_[WB12] + count_[BB12],
                               count_[WR12] + count_[BR12],
                               count_[WQ12] + count_[BQ12]);
    }

    int count(Piece12 pt12) const
    {
        return count_[pt12];
    }

    int count(Side sd, Piece pt) const
    {
        return count(Piece12(2 * pt + sd));
    }

    int count(Piece pt) const
    {
        return count(White, pt) + count(Black, pt);
    }

    int count(Side sd) const
    {
        return bb::count(bb_side_[sd]) - 1;
    }

    bool ocb() const
    {
        u64 wbishops = bb(White, Bishop);
        u64 bbishops = bb(Black, Bishop);

        return (wbishops & bb::Light && bbishops & bb::Dark) || (wbishops & bb::Dark && bbishops & bb::Light);
    }

    std::string pretty() const;

    u64 pinned (Side sd) const { return bb_side_[sd] & pinned_; }
    u64 pinned (       ) const { return pinned_; }
    u64 pinners(Side sd) const { return bb_side_[sd] & pinners_; }
    u64 pinners(       ) const { return pinners_; }

    u64 checkers() const { return checkers_; }
    int checker1() const { return bb::lsb(checkers_); }
    int checker2() const { return bb::msb(checkers_); }

    int phase(       ) const;
    int phase(Side sd) const;

    bool bishop_pair(Side sd) const
    {
        u64 bishops = bb(sd, Bishop);

        return (bishops & bb::Light) && (bishops & bb::Dark);
    }

    Move prev_move() const { return prev_move_; }

    u64 bb(Side sd) const { return bb_side_[sd]; }
    u64 bb(Piece pt) const { return bb_piece_[pt]; }

    u64 occ() const { return bb(White) | bb(Black); }

    u64 bb(Piece pt1, Piece pt2)            const { return bb(pt1) | bb(pt2); }
    u64 bb(Piece pt1, Piece pt2, Piece pt3) const { return bb(pt1, pt2) | bb(pt3); }

    u64 bb(Side sd, Piece pt)                        const { return bb(sd) & bb(pt); }
    u64 bb(Side sd, Piece pt1, Piece pt2)            const { return bb(sd) & bb(pt1, pt2); }
    u64 bb(Side sd, Piece pt1, Piece pt2, Piece pt3) const { return bb(sd) & bb(pt1, pt2, pt3); }

    u64 pawns  (Side sd) const { return bb(sd) & pawns(); }
    u64 minors (Side sd) const { return bb(sd) & minors(); }
    u64 majors (Side sd) const { return bb(sd) & majors(); }
    u64 sliders(Side sd) const { return bb(sd) & sliders(); }
    u64 pieces (Side sd) const { return bb(sd) & pieces(); }

    u64 pawns  ()        const { return bb(Pawn); }
    u64 minors ()        const { return bb(Knight, Bishop); }
    u64 majors ()        const { return bb(Rook, Queen); }
    u64 sliders()        const { return bb(Bishop, Rook, Queen); }
    u64 pieces ()        const { return minors() | majors(); }

    static int see_max(Move m)
    {
        int score = 0;

        score += SEEValue[m.cap9()];

        if (m.is_promo())
            score += SEEValue[m.promo()] - SEEValue[Pawn];

        return score;
    }

    bool see(Move m, int threshold = 0) const;

    u64 attackers_to(u64 occ, int sq) const;
    u64 static_attacks(Side sd) const;

private:
    template <bool UpdateKey> void add_piece(int sq, Piece12 pt12);
    template <bool UpdateKey> void rem_piece(int sq);
    template <bool UpdateKey> void mov_piece(int orig, int dest);

    void set_pins();
    u64 get_checkers() const;

    void set_pins_checkers();

    u64 bb_side_[2]     = { };
    u64 bb_piece_[6]    = { };

    u64 key_            = 0;
    u64 pinned_         = 0;
    u64 pinners_        = 0;
    u64 checkers_       = 0;

    Move prev_move_     = Move::None();

    i8 count_[12]       = { };
    Piece12 square_[64];

    Side side_          = None;
    u8 flags_           = 0;
    u8 ep_sq_           = square::None;
    u8 half_moves_      = 0;
    u16 full_moves_     = 1;

    i8 king_[2]         = { };
};

#endif
