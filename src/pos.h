#ifndef POS_H
#define POS_H

#include <bitset>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "move.h"
#include "piece.h"
#include "square.h"
#include "types.h"
#include "util.h"
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

    Position() { }
    Position(const std::string fen, double outcome = 1);
    Position(const char fen[]) : Position(std::string(fen)) { }

    Move note_move(const std::string& s) const;

    void   make_move(const Move& move,       MoveUndo& undo);
    void unmake_move(const Move& move, const MoveUndo& undo);
   
    void   make_null(      NullUndo& undo);
    void unmake_null(const NullUndo& undo);
    
    std::string to_fen() const;

    int is_ok(bool check_test = true) const;

    u64 key() const { return key_; }
    u64 key(u64 key) { return key_ = key; }

    u64 pawn_key() const { return pawn_key_; }

    u64 calc_key() const;

    void get_board(u8* board) const { memcpy(board, square_, 16 * 12); }

    void mark_pins(BitSet& pins) const;

    bool move_is_check       (const Move& move) const;
    bool move_is_legal       (const Move& move) const;
    bool move_is_legal_ep    (const Move& move) const;
    bool move_is_recap       (const Move& move) const;

    bool ep_is_valid    (int sq)             const;
    bool side_attacks   (int side, int dest) const;
    bool piece_attacks  (int orig, int dest) const;

    int see_est(const Move& move) const;
    int see_max(const Move& move) const;

    u8 at(int sq) const
    {
        assert(sq >= -36 && sq < 156);

        return square_[36 + sq];
    }

    const u8& operator[](int sq) const
    {
        assert(sq >= -36 && sq < 156);

        return square(sq);
    }

    const u8& square(int sq) const
    {
        assert(sq >= -36 && sq < 156);

        return square_[36 + sq];
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

        return P256ToP6[square(sq)] == piece;
    }

    const PieceList& plist(int p12) const
    {
        assert(piece12_is_ok(p12));

        return plist_[p12];
    }

    bool has_bishop_on(int side, int color) const;

    int king_sq(int side) const
    {
        assert(side_is_ok(side));

        return plist_[WK12 + side][0];
    }

    int king_sq() const { return king_sq(side_); }

    void king_zone(std::bitset<192>& bs, int side) const;

    u8 side()       const { return side_; }
    u8 cap_sq()     const { return cap_sq_; }
    u8 ep_sq()      const { return ep_sq_; }
    u8 ep_sq(u8 sq)       { return ep_sq_ = sq; }

    u8  half_moves()  const { return half_moves_; }
    u16 full_moves()  const { return full_moves_; }

    bool can_castle_k(int side) const { return (flags_ & (WhiteCastleKFlag << side)) != 0; }
    bool can_castle_q(int side) const { return (flags_ & (WhiteCastleQFlag << side)) != 0; }
    bool can_castle  (int side) const { return (flags_ & (WhiteCastleFlags << side)) != 0; }
    bool can_castle_k(        ) const { return can_castle_k(side_); }
    bool can_castle_q(        ) const { return can_castle_q(side_); }
    bool can_castle  (        ) const { return can_castle(side_); }

    // draw by insufficient material

    bool draw_mat_insuf(        ) const;
    bool draw_mat_insuf(int side) const;
    bool non_pawn_mat  (        ) const;
    bool non_pawn_mat  (int side) const;

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

    std::string dump() const;

    u8 checkers() const
    {
        return checkers_count_;
    }

    u8 checkers(int i) const
    {
        assert(i >= 0 && i < checkers_count_);

        return checkers_sq_[i];
    }

    bool in_check() const { return checkers() > 0; }

    int phase() const;
    
    int pawns  (int side)   const { return plist(WP12 + side).size(); }
    int knights(int side)   const { return plist(WN12 + side).size(); }
    int bishops(int side)   const { return plist(WB12 + side).size(); }
    int rooks  (int side)   const { return plist(WR12 + side).size(); }
    int queens (int side)   const { return plist(WQ12 + side).size(); }
    int minors (int side)   const { return knights(side) + bishops(side); }
    int majors (int side)   const { return rooks(side) + queens(side); }
    int pieces (int side)   const { return knights(side) + bishops(side) + rooks(side); }

    int pawns()             const { return pawns(White) + pawns(Black); }
    int knights()           const { return knights(White) + knights(Black); }
    int bishops()           const { return bishops(White) + bishops(Black); }
    int rooks()             const { return rooks(White) + rooks(Black); }
    int queens()            const { return queens(White) + queens(Black); }
    int minors()            const { return minors(White) + minors(Black); }
    int majors()            const { return majors(White) + majors(Black); }
    int pieces()            const { return pieces(White) + pieces(Black); }
    
    int dsbishops(int side) const;
    int lsbishops(int side) const;

    void swap_sides();

    u8 captured_piece(const Move& m) const
    {
        assert(m.is_capture());
        assert(is_op(m.dest()));

        return square(m.dest());
    }

    u8 capturing_piece(const Move& m) const
    {
        assert(m.is_capture());
        assert(is_me(m.orig()));

        return square(m.orig());
    }

    bool bishop_pair(int side) const;

    friend bool operator==(const Position& lhs, const Position& rhs);

    size_t plists() const { return sizeof(plist_); }

    double outcome() const { return outcome_; }

private:
    template <bool UpdateKey> void add_piece(int sq, u8 piece);
    template <bool UpdateKey> void rem_piece(int sq);
    template <bool UpdateKey> void mov_piece(int orig, int dest);

    void set_checkers_slow();
    void set_checkers_fast(const Move& move);

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

    u8 side_            = -1;
    u8 ep_sq_           = SquareNone;
    u8 cap_sq_          = SquareNone;
    u8 half_moves_      = 0;
    u16 full_moves_     = 1;

    u8 checkers_count_  = 0;
    u8 checkers_sq_[2];

    double outcome_ = 0;
};

#endif
