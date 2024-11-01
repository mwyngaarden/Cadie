#ifndef POS_H
#define POS_H

#include <bit>
#include <bitset>
#include <string>
#include <tuple>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "bb.h"
#include "misc.h"
#include "move.h"
#include "piece.h"
#include "square.h"
#include "zobrist.h"

class PositionBin
{
public:
    u64 get(size_t pos, size_t bits) const
    {
        size_t index  = pos >> 6;
        size_t offset = pos & 63;
        
        return (data_[index] >> offset) & ((1 << bits) - 1);
    }
    
    void set(size_t pos, u64 n)
    {
        size_t index  = pos >> 6;
        size_t offset = pos & 63;
        
        data_[index] |= n << offset;
    }

    const char * get() const
    {
        return reinterpret_cast<const char *>(data_);
    }

private:
    u64 data_[5] = { };
};

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
    Position(const std::string fen, int score1 = 0, int score2 = 0);
    Position(const char fen[]) : Position(std::string(fen)) { }
    Position(const PositionBin& bin);

    Move note_move(const std::string& s) const;

    void   make_move(Move m, bool skip_checkers = false);
    void unmake_move(const UndoInfo& undo);

    void   make_null();
    void unmake_null(const UndoInfo& undo);

    void make_move_fast(Move m);

    UndoInfo undo_info() const
    {
        UndoInfo undo;

        undo.key            = key_;
        undo.pins           = pins_;
        undo.checkers       = checkers_;
        //undo.pawn_key       = pawn_key_;
        undo.flags          = flags_;
        undo.ep_sq          = ep_sq_;
        undo.half_moves     = half_moves_;
        undo.full_moves     = full_moves_;
        undo.prev_move      = prev_move_;

        return undo;
    }

    std::string to_fen() const;

    u64 key() const { return key_; }

    //u64 pawn_key() const { return pawn_key_; }

    bool move_is_check      (Move m);
    bool move_is_recap      (Move m) const;
    bool move_is_legal      (Move m) const;
    bool move_is_sane       (Move m);

    int mvv_lva             (Move m) const;

    bool ep_is_valid        (side_t side, int sq) const;
    bool side_attacks       (side_t side, int dest) const;

    u8 board(int sq) const
    {
        assert(sq64_is_ok(sq));

        return board_[sq];
    }

    template <int I>
    int board(int sq) const
    {
        static_assert(I == 6 || I == 12);

        u8 piece = board_[sq];

        return I == 6 ? P256ToP6[piece] : P256ToP12[piece];
    }

    bool empty(int sq) const
    {
        assert(sq64_is_ok(sq));

        return !bb::test(occ(), sq);
    }

    int count() const { return std::popcount(occ()); }

    bool line_is_empty(int orig, int dest) const;

    int king(side_t side) const
    {
        assert(side_is_ok(side));

        return bb::lsb(bb(side, King));
    }

    int king() const { return king(side_); }

    side_t side() const { return side_; }

    bool can_castle_k()            const { return (flags_ & (WhiteCastleKFlag << side_)) != 0; }
    bool can_castle_q()            const { return (flags_ & (WhiteCastleQFlag << side_)) != 0; }
    bool can_castle_k(side_t side) const { return (flags_ & (WhiteCastleKFlag << side )) != 0; }
    bool can_castle_q(side_t side) const { return (flags_ & (WhiteCastleQFlag << side )) != 0; }

    u8 flags() const { return flags_; }

    u8 ep_sq() const { return ep_sq_; }
    u8 ep_sq(u8 sq) { return ep_sq_ = sq; }

    u8  half_moves() const { return half_moves_; }
    u16 full_moves() const { return full_moves_; }

    bool draw_dead  (side_t side) const;
    bool draw_dead  () const;
    bool draw_fifty () const;
    bool draw_rep   () const;
    bool draw       () const;

    int draw_scale(int eval) const { return (200 - half_moves_) * eval / 200; }

    void counts(int& wp, int& wn, int& wb, int& wr, int& wq,
                int& bp, int& bn, int& bb, int& br, int& bq) const
    {
        wp = pawns(White);
        wn = knights(White);
        wb = bishops(White);
        wr = rooks(White);
        wq = queens(White);

        bp = pawns(Black);
        bn = knights(Black);
        bb = bishops(Black);
        br = rooks(Black);
        bq = queens(Black);
    }

    bool ocb() const
    {
        u64 wbb = bb(White, Bishop);
        u64 bbb = bb(Black, Bishop);

        return (wbb & bb::Light && bbb & bb::Dark) || (wbb & bb::Dark && bbb & bb::Light);
    }

    std::string pretty() const;

    u64 pins() const { return pins_; }

    u64 checkers() const { return checkers_; }
    int checker1() const { return bb::lsb(checkers_); }
    int checker2() const { return bb::msb(checkers_); }

    int phase(           ) const;
    int phase(side_t side) const;

    bool bishop_pair(side_t side) const
    {
        assert(side_is_ok(side));

        u64 b = bb(side, Bishop);

        return (b & bb::Light) && (b & bb::Dark);
    }

    Move prev_move() const { return prev_move_; }

    u64 occ() const { return bb_side_[White] | bb_side_[Black]; }

    u64 bb(side_t side) const
    {
        assert(side_is_ok(side));
        return bb_side_[side];
    }

    u64 bb(int type) const
    {
        assert(piece_is_ok(type));
        return bb_pieces_[type];
    }

    u64 bb(side_t side, int type) const
    {
        assert(side_is_ok(side));
        assert(piece_is_ok(type));
        return bb_side_[side] & bb_pieces_[type];
    }

    // FIXME naming convention
    int pawns  (side_t side) const { return std::popcount(bb(side, Pawn)); }
    int knights(side_t side) const { return std::popcount(bb(side, Knight)); }
    int bishops(side_t side) const { return std::popcount(bb(side, Bishop)); }
    int rooks  (side_t side) const { return std::popcount(bb(side, Rook)); }
    int queens (side_t side) const { return std::popcount(bb(side, Queen)); }

    int pawns  ()            const { return std::popcount(bb(Pawn)); }
    int knights()            const { return std::popcount(bb(Knight)); }
    int bishops()            const { return std::popcount(bb(Bishop)); }
    int rooks  ()            const { return std::popcount(bb(Rook)); }
    int queens ()            const { return std::popcount(bb(Queen)); }

    u64 minors (side_t side) const { return bb(side) & minors(); }
    u64 majors (side_t side) const { return bb(side) & majors(); }
    u64 sliders(side_t side) const { return bb(side) & sliders(); }
    u64 pieces (side_t side) const { return bb(side) & pieces(); }

    u64 minors ()            const { return bb_pieces_[Knight] | bb_pieces_[Bishop]; }
    u64 majors ()            const { return bb_pieces_[Rook] | bb_pieces_[Queen]; }
    u64 sliders()            const { return bb_pieces_[Bishop] | bb_pieces_[Rook] | bb_pieces_[Queen]; }
    u64 pieces ()            const { return minors() | majors(); }

    std::string to_egtb() const;

    static int see_max(Move m)
    {
        int score = 0;

        if (m.is_capture()) {
            int type = P256ToP6[m.captured()];
            score += SEEValue[type];
        }

        if (m.is_promo()) {
            int type = m.promo();
            score += SEEValue[type] - SEEValue[Pawn];
        }

        return score;
    }

    bool see(Move m, int threshold = 0) const;

    u64 attackers_to(u64 occ, int sq) const;

#ifdef TUNE 
    int score1() const { return score1_; }
    int score2() const { return score2_; }

    PositionBin serialize() const;

    void deserialize(const PositionBin& bin);
#endif

private:
    template <bool UpdateKey> void add_piece(int sq, u8 piece);
    template <bool UpdateKey> void rem_piece(int sq);
    template <bool UpdateKey> void mov_piece(int orig, int dest);

    u64 get_pins() const;
    u64 get_checkers() const;
    
    u8& board(int sq)
    {
        assert(sq64_is_ok(sq));

        return board_[sq];
    }
    
    u64 bb_side_[2]     = { };
    u64 bb_pieces_[6]   = { };

    u8 board_[64]       = { };

    u64 key_            = 0;
    u64 pins_           = 0;
    u64 checkers_       = 0;
    //u64 pawn_key_       = 0;

    Move prev_move_     = Move::None();

    side_t side_        = 2;
    u8 flags_           = 0;
    u8 ep_sq_           = square::None;
    u8 half_moves_      = 0;
    u16 full_moves_     = 1;


#ifdef TUNE
    i16 score1_;
    i16 score2_;
#endif
};

#endif
