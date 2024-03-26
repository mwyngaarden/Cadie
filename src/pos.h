#ifndef POS_H
#define POS_H

#include <bit>
#include <bitset>
#include <string>
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
    Position(const std::string fen, double outcome = 1);
    Position(const char fen[]) : Position(std::string(fen)) { }

    Move note_move(const std::string& s) const;

    bool   make_move_hack(const Move& m, bool test);
    void unmake_move_hack(const Move& m, u8 csq1, u8 csq2, u8 c);
    void   make_move     (const Move& m);
    void   make_move     (const Move& m,       UndoMove& undo);
    void unmake_move     (const Move& m, const UndoMove& undo);
   
    void   make_null(      UndoNull& undo);
    void unmake_null(const UndoNull& undo);
    
    std::string to_fen() const;

    u64 key() const { return key_; }

    u64 pawn_key() const { return pawn_key_; }

    bool move_is_check      (const Move& m) const;
    bool move_is_legal      (const Move& m) const;
    bool move_is_recap      (const Move& m) const;
    bool move_is_safe       (const Move& m, int& see, int threshold = 0) const;
    bool move_is_dangerous  (const Move& m, bool gives_check) const;

    int mvv_lva             (const Move& m) const;

    bool ep_is_valid        (int sq) const;
    bool side_attacks       (side_t side, int dest) const;
    bool piece_attacks      (int orig, int dest) const;

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
  
    bool empty64(int sq) const
    {
        assert(sq64_is_ok(sq));

        return board_[sq] == PieceNone256;
    }
    
    bool line_is_empty(int orig, int dest) const;

    template <side_t Side>
    bool is_side(int sq) const
    {
        assert(sq88_is_ok(sq));

        return board(to_sq64(sq)) & make_flag(Side);
    }
    
    bool is_side(int sq, side_t side) const
    {
        assert(sq88_is_ok(sq));
        assert(side_is_ok(side));

        return board(to_sq64(sq)) & make_flag(side);
    }

    bool is_piece(int sq, int piece) const
    {
        assert(sq88_is_ok(sq));
        assert(piece_is_ok(piece));

        return board<6>(to_sq64(sq)) == piece;
    }

    int king(side_t side) const
    {
        assert(side_is_ok(side));

        return to_sq88(std::countr_zero(bb(side, King)));
    }

    int king() const { return king(side_); }
    
    int king64(side_t side) const
    {
        assert(side_is_ok(side));

        return std::countr_zero(bb(side, King));
    }

    int king64() const { return king64(side_); }

    side_t side() const { return side_; }

    bool can_castle_k()            const { return (flags_ & (WhiteCastleKFlag << side_)) != 0; }
    bool can_castle_q()            const { return (flags_ & (WhiteCastleQFlag << side_)) != 0; }
    bool can_castle_k(side_t side) const { return (flags_ & (WhiteCastleKFlag << side )) != 0; }
    bool can_castle_q(side_t side) const { return (flags_ & (WhiteCastleQFlag << side )) != 0; }

    u8 ep_sq() const { return ep_sq_; }
    u8 ep_sq(u8 sq) { return ep_sq_ = sq; }

    u8  half_moves() const { return half_moves_; }
    u16 full_moves() const { return full_moves_; }

    bool draw_mat_insuf(           ) const;
    bool draw_mat_insuf(side_t side) const;
    bool non_pawn_mat  (           ) const;
    bool non_pawn_mat  (side_t side) const;

    void counts(int& p, int& n, int& lb, int& db, int& r, int& q, side_t side) const
    {
        p  = counts_[WP12 + side];
        n  = counts_[WN12 + side];
        lb = lsbishops(side);
        db = dsbishops(side);
        r  = counts_[WR12 + side];
        q  = counts_[WQ12 + side];
    }

    bool ocb() const
    {
        return (lsbishops(White) && dsbishops(Black)) || (dsbishops(White) && lsbishops(Black));
    }

    std::string pretty() const;

    u8 checkers() const
    {
        return checkers_;
    }

    u8 checkers(int i) const
    {
        assert(i >= 0 && i < checkers_);

        return checkers_sq_[i];
    }

    int phase    (           ) const;
    int phase_inv(side_t side) const;
    
    int pawns  (side_t side) const { return counts_[WP12 + side]; }
    int knights(side_t side) const { return counts_[WN12 + side]; }
    int bishops(side_t side) const { return counts_[WB12 + side]; }
    int rooks  (side_t side) const { return counts_[WR12 + side]; }
    int queens (side_t side) const { return counts_[WQ12 + side]; }

    int minors (side_t side) const { return knights(side) + bishops(side); }
    int majors (side_t side) const { return rooks  (side) + queens (side); }
    int pieces (side_t side) const { return knights(side) + bishops(side) + rooks(side); }

    int pawns  () const { return pawns  (White) + pawns  (Black); }
    int knights() const { return knights(White) + knights(Black); }
    int bishops() const { return bishops(White) + bishops(Black); }
    int rooks  () const { return rooks  (White) + rooks  (Black); }
    int queens () const { return queens (White) + queens (Black); }
    int minors () const { return minors (White) + minors (Black); }
    int majors () const { return majors (White) + majors (Black); }
    int pieces () const { return pieces (White) + pieces (Black); }
    
    int lsbishops(side_t side) const
    {
        assert(side_is_ok(side));
        return std::popcount(bb(side, Bishop) & bb::Light);
    }
    
    int dsbishops(side_t side) const
    {
        assert(side_is_ok(side));
        return std::popcount(bb(side, Bishop) & bb::Dark);
    }

    bool bishop_pair(side_t side) const
    {
        assert(side_is_ok(side));
        return lsbishops(side) && dsbishops(side);
    }

    Move move_prev() const { return move_prev_; }

    u64 occ(           ) const { return bb_all_; }
    u64 occ(side_t side) const { return bb_side_[side]; }

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
   
#ifdef TUNE
    double outcome() const { return outcome_; }
#endif

private:
    template <bool UpdateKey> void add_piece(int sq, u8 piece);
    template <bool UpdateKey> void rem_piece(int sq);
    template <bool UpdateKey> void mov_piece(int orig, int dest);

    void set_checkers();
    
    u8& board(int sq)
    {
        assert(sq64_is_ok(sq));

        return board_[sq];
    }
    
    u64 bb_side_[2]     = { };
    u64 bb_pieces_[6]   = { };
    u64 bb_all_         = 0;
    int counts_[12]     = { };

    u8 board_[64];
    
    u64 key_            = 0;
    u64 pawn_key_       = 0;

    u8 flags_           = 0;

    side_t side_        = 2;
    u8 ep_sq_           = SquareNone;
    u8 half_moves_      = 0;
    u16 full_moves_     = 1;

    u8 checkers_        = 0;
    u8 checkers_sq_[2];

    Move move_prev_     = MoveNone;

#ifdef TUNE
    double outcome_     = 0;
#endif
};

#endif
