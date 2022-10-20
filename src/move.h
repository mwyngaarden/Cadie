#ifndef MOVE_H
#define MOVE_H

#include <limits>
#include <sstream>
#include <string>
#include <cassert>
#include <cstdint>
#include "piece.h"
#include "square.h"
#include "types.h"
#include "util.h"

class Move {
public:
    static constexpr u32 CaptureFlags       = 0xff   << 14;

    static constexpr u32 PromoKnightFlag    = Knight << 22;
    static constexpr u32 PromoBishopFlag    = Bishop << 22;
    static constexpr u32 PromoRookFlag      = Rook   << 22;
    static constexpr u32 PromoQueenFlag     = Queen  << 22;
    static constexpr u32 PromoFlags         = PromoQueenFlag | PromoRookFlag | PromoBishopFlag | PromoKnightFlag;

    static constexpr u32 DoubleFlag         = 1 << 25;
    static constexpr u32 CastleFlag         = 1 << 26;
    static constexpr u32 EPFlag             = 1 << 27;

    Move(        )               { }
    Move(u32 data) : data_(data) { }

    Move(int orig, int dest, u8 piece = PieceNone256)
    {
        assert(sq88_is_ok(orig));
        assert(sq88_is_ok(dest));
        assert(!is_king(piece));

        data_ = (piece << 14) | (dest << 7) | orig;
    }

    void set_double()           { data_ |= DoubleFlag; }
    void set_castle()           { data_ |= CastleFlag; }
    void set_ep()               { data_ |= EPFlag; }
    void set_capture(u8 piece)  { data_ |= piece << 14; }
    void set_promo(u32 flag)    { data_ |= flag; }
    
    operator u32()          const { return data_; }

    int orig()              const { return  data_        & 0x7f; }
    int dest()              const { return (data_ >>  7) & 0x7f; }

    u8 capture_piece()      const { return (data_ >> 14) & 0xff; }
    int promo_piece()       const { return (data_ >> 22) & 0x07; }

    bool is_capture()       const { return data_ & CaptureFlags; }
    bool is_tactical()      const { return data_ & (EPFlag | CaptureFlags | PromoFlags); }
    bool is_promo()         const { return data_ & PromoFlags; }
    bool is_under()         const { return (data_ & PromoFlags) && (data_ & PromoFlags) != PromoQueenFlag ; }
    bool is_castle()        const { return data_ & CastleFlag; }
    bool is_ep()            const { return data_ & EPFlag; }
    bool is_double()        const { return data_ & DoubleFlag; }

    std::string to_string() const
    {
        std::ostringstream oss;

        oss << sq88_to_san(orig());
        oss << sq88_to_san(dest());

             if (promo_piece() == Knight) oss << 'n';
        else if (promo_piece() == Bishop) oss << 'b';
        else if (promo_piece() == Rook) oss << 'r';
        else if (promo_piece() == Queen) oss << 'q';

        return oss.str();
    }

    // only sets orig, dest, and promotion piece!

    static Move from_string(std::string s)
    {
        assert(s.size() == 4 || s.size() == 5);

        const int ffile = s[0] - 'a';
        const int frank = s[1] - '1';
        const int tfile = s[2] - 'a';
        const int trank = s[3] - '1';

        assert(file_is_ok(ffile));
        assert(rank_is_ok(frank));
        assert(file_is_ok(tfile));
        assert(rank_is_ok(trank));

        const int orig = to_sq88(ffile, frank);
        const int dest = to_sq88(tfile, trank);

        assert(sq88_is_ok(orig));
        assert(sq88_is_ok(dest));

        Move move(orig, dest);

        if (s.size() == 5) {
                 if (s[4] == 'n') move.set_promo(PromoKnightFlag);
            else if (s[4] == 'b') move.set_promo(PromoBishopFlag);
            else if (s[4] == 'r') move.set_promo(PromoRookFlag);
            else if (s[4] == 'q') move.set_promo(PromoQueenFlag);
            else assert(false);
        }

        return move;
    }

private:
    u32 data_;
};

struct MoveUndo {
    u64 key;
    u64 pawn_key;
    u16 full_moves;
    u8 flags;
    u8 ep_sq;
    u8 cap_sq;
    u8 half_moves;
    u8 checkers_sq[2];
    u8 checkers_count;
};

struct NullUndo {
    u64 pawn_key;
    u8 ep_sq;
    u8 cap_sq;
    u8 checkers_sq[2];
    u8 checkers_count;
};

using MoveList  = Util::List<Move, MovesMax>;
using MoveStack = Util::List<Move, 1024>;
using PV        = Move[PliesMax];

const Move MoveNone(0, 0);
const Move MoveNull(1, 1);

#endif
