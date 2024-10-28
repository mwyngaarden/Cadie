#ifndef MOVE_H
#define MOVE_H

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <sstream>
#include <string>
#include <cassert>
#include <cstdint>
#include "list.h"
#include "misc.h"
#include "piece.h"
#include "square.h"

class Move {
public:
    static constexpr Move None() { return Move(0, 0); }
    static constexpr Move Null() { return Move(1, 1); }

    static constexpr u32 CaptureFlags   = 0xff   << 12;

    static constexpr u32 PromoNFlag     = Knight << 20;
    static constexpr u32 PromoBFlag     = Bishop << 20;
    static constexpr u32 PromoRFlag     = Rook   << 20;
    static constexpr u32 PromoQFlag     = Queen  << 20;
    static constexpr u32 PromoUFlags    = PromoNFlag | PromoBFlag | PromoRFlag;
    static constexpr u32 PromoFlags     = PromoUFlags | PromoQFlag;

    static_assert(std::has_single_bit(PromoQFlag));
    static_assert((PromoQFlag & PromoUFlags) == 0);

    static constexpr u32 EPFlag         = 1 << 23;
    static constexpr u32 SingleFlag     = 1 << 24;
    static constexpr u32 DoubleFlag     = 1 << 25;
    static constexpr u32 CastleFlag     = 1 << 26;
    static constexpr u32 IrrevFlag      = CaptureFlags | PromoFlags | EPFlag | SingleFlag | DoubleFlag;

    static_assert((EPFlag & PromoFlags) == 0);

    constexpr Move() = default;
    constexpr Move(u32 data) : data_(data) { }
    
    constexpr Move(int orig, int dest)
    {
        assert(sq64_is_ok(orig));
        assert(sq64_is_ok(dest));

        data_ = (dest << 6) | orig;
    }

    constexpr Move(int orig, int dest, u8 cap)
    {
        assert(sq64_is_ok(orig));
        assert(sq64_is_ok(dest));
        assert(!is_king(cap));

        data_ = (cap << 12) | (dest << 6) | orig;
    }

    void set_capture(u8 cap)  { data_ |= cap << 12; }
    void set_flag(u32 flag)   { data_ |= flag; }
    
    operator u32()          const { return data_; }

    int orig()              const { return  data_       & 0x3f; }
    int dest()              const { return (data_ >> 6) & 0x3f; }

    u8 captured()           const { return (data_ >> 12) & 0xff; }
    int promo()             const { return (data_ >> 20) & 0x07; }

    bool is_capture()       const { return data_ & CaptureFlags; }
    bool is_tactical()      const { return data_ & (EPFlag | CaptureFlags | PromoFlags); }
    bool is_promo()         const { return data_ & PromoFlags; }
    bool is_under_promo()   const { return data_ & PromoUFlags; }
    bool is_queen_promo()   const { return data_ & PromoQFlag; }
    bool is_ep()            const { return data_ & EPFlag; }
    bool is_single()        const { return data_ & SingleFlag; }
    bool is_double()        const { return data_ & DoubleFlag; }
    bool is_castle()        const { return data_ & CastleFlag; }
    bool is_valid()         const { return orig() != dest(); }
    bool is_irrev()         const { return data_ & IrrevFlag; }
    bool is_special()       const { return data_ & (PromoFlags | EPFlag | CastleFlag | DoubleFlag); }

    int index(        )     const { return  data_ & 0xfff; }
    int index(int side)     const { return (data_ & 0xfff) | (side << 12); }

    std::string str() const
    {
        std::ostringstream oss;

        oss << square::sq_to_san(orig());
        oss << square::sq_to_san(dest());

             if (promo() == Knight) oss << 'n';
        else if (promo() == Bishop) oss << 'b';
        else if (promo() == Rook) oss << 'r';
        else if (promo() == Queen) oss << 'q';

        return oss.str();
    }

    // Only sets orig, dest, and promotion piece!

    static Move from_string(std::string s)
    {
        int ofile = s[0] - 'a';
        int orank = s[1] - '1';
        int dfile = s[2] - 'a';
        int drank = s[3] - '1';

        int orig = to_sq64(ofile, orank);
        int dest = to_sq64(dfile, drank);

        Move move(orig, dest);

        if (s.size() == 5) {
                 if (s[4] == 'n') move.set_flag(PromoNFlag);
            else if (s[4] == 'b') move.set_flag(PromoBFlag);
            else if (s[4] == 'r') move.set_flag(PromoRFlag);
            else if (s[4] == 'q') move.set_flag(PromoQFlag);
        }

        return move;
    }

private:
    u32 data_;
};

struct UndoInfo {
    u64 key;
    u64 pins;
    u64 checkers;
    //u64 pawn_key;
    u16 full_moves;
    u8 flags;
    u8 ep_sq;
    u8 half_moves;
    Move prev_move;
};

using MoveList  = List<Move, MovesMax>;
using MoveStack = List<Move, 1024>;
using PV        = Move[PliesMax];

#endif
