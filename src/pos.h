#ifndef POS_H
#define POS_H

#include <bitset>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
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

    void   make_move(const Move& m,       UndoMove& undo);
    void unmake_move(const Move& m, const UndoMove& undo);
   
    void   make_null(      UndoNull& undo);
    void unmake_null(const UndoNull& undo);
    
    std::string to_fen() const;

    u64 key() const { return key_; }
    u64 key(u64 key) { return key_ = key; }

    u64 pawn_key() const { return pawn_key_; }

    u64 calc_key() const;

    void get_board(u8* board) const { memcpy(board, square_, 16 * 12); }

    void mark_pins(std::bitset<128>& pins) const;

    bool move_is_check      (const Move& m) const;
    bool move_is_legal      (const Move& m) const;
    bool move_is_legal_ep   (const Move& m) const;
    bool move_is_recap      (const Move& m) const;
    bool move_is_safe       (const Move& m, int& see) const;
    bool move_is_dangerous  (const Move& m, bool checks) const;

    int see_max             (const Move& m) const;
    int mvv_lva             (const Move& m) const;

    bool ep_is_valid    (int sq) const;
    bool side_attacks   (side_t side, int dest) const;
    bool piece_attacks  (int orig, int dest) const;

    const u8& square(int sq) const
    {
        assert(sq >= -36 && sq < 156);

        return square_[36 + sq];
    }

    const u8& operator[](int sq) const
    {
        return square(sq);
    }
   
    template <int I>
    int square(int sq) const
    {
        static_assert(I == 6 || I == 12);

        u8 piece = square(sq);

        return I == 6 ? P256ToP6[piece] : P256ToP12[piece];
    }
  
    bool empty(int sq) const
    {
        assert(sq88_is_ok(sq));

        return square(sq) == PieceNone256;
    }

    bool ray_is_empty(int orig, int dest) const;

    bool is_op(int sq) const
    {
        assert(sq88_is_ok(sq));

        return square(sq) & (BlackFlag256 >> side_);
    }

    bool is_me(int sq) const
    {
        assert(sq88_is_ok(sq));

        return square(sq) & (WhiteFlag256 << side_);
    }

    bool is_piece(int sq, int piece) const
    {
        assert(sq88_is_ok(sq));
        assert(piece_is_ok(piece));

        return square<6>(sq) == piece;
    }

    const PieceList& plist(int p12) const
    {
        assert(piece12_is_ok(p12));

        return plist_[p12];
    }

    int king(side_t side) const
    {
        assert(side_is_ok(side));

        return plist_[WK12 + side][0];
    }

    int king() const { return king(side_); }

    void king_zone(std::bitset<192>& bs, side_t side) const;

    side_t side() const { return side_; }

    bool can_castle_k() const { return (flags_ & (WhiteCastleKFlag << side_)) != 0; }
    bool can_castle_q() const { return (flags_ & (WhiteCastleQFlag << side_)) != 0; }

    u8 ep_sq() const { return ep_sq_; }
    u8 ep_sq(u8 sq) { return ep_sq_ = sq; }

    u8  half_moves() const { return half_moves_; }
    u16 full_moves() const { return full_moves_; }

    bool draw_mat_insuf(           ) const;
    bool draw_mat_insuf(side_t side) const;
    bool non_pawn_mat  (           ) const;
    bool non_pawn_mat  (side_t side) const;

    void counts(int& wp, int& wn, int& wb, int& wr, int& wq, 
                int& bp, int& bn, int& bb, int& br, int& bq) const
    {
        wp  = plist(WP12).size();
        wn  = plist(WN12).size();
        wb  = plist(WB12).size();
        wr  = plist(WR12).size();
        wq  = plist(WQ12).size();

        bp  = plist(BP12).size();
        bn  = plist(BN12).size();
        bb  = plist(BB12).size();
        br  = plist(BR12).size();
        bq  = plist(BQ12).size();
    }
    
    void counts(int& wp, int& wn, int& wlb, int& wdb, int& wr, int& wq, 
                int& bp, int& bn, int& blb, int& bdb, int& br, int& bq) const
    {
        wp  = plist(WP12).size();
        wn  = plist(WN12).size();
        wlb = lsbishops(White);
        wdb = dsbishops(White);
        wr  = plist(WR12).size();
        wq  = plist(WQ12).size();

        bp  = plist(BP12).size();
        bn  = plist(BN12).size();
        blb = lsbishops(Black);
        bdb = dsbishops(Black);
        br  = plist(BR12).size();
        bq  = plist(BQ12).size();
    }

    std::string to_egtb() const;

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

    int phase() const;
    
    int pawns  (side_t side) const { return plist(WP12 + side).size(); }
    int knights(side_t side) const { return plist(WN12 + side).size(); }
    int bishops(side_t side) const { return plist(WB12 + side).size(); }
    int rooks  (side_t side) const { return plist(WR12 + side).size(); }
    int queens (side_t side) const { return plist(WQ12 + side).size(); }

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
    
    int dsbishops(side_t side) const;
    int lsbishops(side_t side) const;

    void swap_sides();

    template <side_t Side>
    bool bishop_pair() const
    {
        u8 flags = 0;

        for (auto sq : plist(WB12 + Side))
            flags |= 1 << sq88_color(sq);

        return flags == 3;
    }

    double outcome() const { return outcome_; }

    Move pm() const { return pm_; }

private:
    template <bool UpdateKey> void add_piece(int sq, u8 piece);
    template <bool UpdateKey> void rem_piece(int sq);
    template <bool UpdateKey> void mov_piece(int orig, int dest);

    void set_checkers_slow();
    void set_checkers_fast(const Move& m);

    u8& square(int sq)
    {
        assert(sq >= -36 && sq < 156);

        return square_[36 + sq];
    }
    
    PieceList plist_[12];
    
    u64 key_            = 0;
    u64 pawn_key_       = 0;

    u8 square_[16 * 12];

    u8 flags_           = 0;

    side_t side_        = SideCount;
    u8 ep_sq_           = SquareNone;
    u8 half_moves_      = 0;
    u16 full_moves_     = 1;

    u8 checkers_        = 0;
    u8 checkers_sq_[2];

    Move pm_            = MoveNone;

    double outcome_ = 0;
};

#endif
