#ifndef MOVE_H
#define MOVE_H

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <sstream>
#include <string>
#include <cstdint>
#include "list.h"
#include "misc.h"
#include "piece.h"
#include "square.h"

class Move {
public:
    static constexpr Move None() { return Move(0); }
    static constexpr Move Null() { return Move(NonCapFlag); }

    static constexpr u32 NonCapFlag     = None12 << 12;
    static constexpr u32 CaptureFlags   = 0x1f   << 12;

    static constexpr u32 PromoNFlag     = Knight << 20;
    static constexpr u32 PromoBFlag     = Bishop << 20;
    static constexpr u32 PromoRFlag     = Rook   << 20;
    static constexpr u32 PromoQFlag     = Queen  << 20;
    static constexpr u32 PromoUFlags    = PromoNFlag | PromoBFlag | PromoRFlag;
    static constexpr u32 PromoFlags     = PromoUFlags | PromoQFlag;

    static constexpr u32 EPFlag         = 1 << 23;
    static constexpr u32 SingleFlag     = 1 << 24;
    static constexpr u32 DoubleFlag     = 1 << 25;
    static constexpr u32 CastleFlag     = 1 << 26;
    static constexpr u32 IrrevFlag      = NonCapFlag | PromoFlags | EPFlag | SingleFlag | DoubleFlag;
    static constexpr u32 SpecialFlags   = PromoFlags | EPFlag | CastleFlag | DoubleFlag;
    static constexpr u32 TacticalFlags  = NonCapFlag | EPFlag | PromoFlags;

    constexpr Move() = default;
    constexpr Move(u32 data) : data_(data) { }
    
    constexpr Move(int orig, int dest)
    {
        data_ = (None12 << 12) | (dest << 6) | orig;
    }

    constexpr Move(int orig, int dest, Piece12 cap12)
    {
        data_ = (cap12 << 12) | (dest << 6) | orig;
    }

    void set_capture(Piece12 cap12) { data_ = (data_ & ~CaptureFlags) | (cap12 << 12); }
    void set_flag(u32 flag)         { data_ |= flag; }
    
    operator u32()          const { return data_; }

    int orig()              const { return  data_       & 0x3f; }
    int dest()              const { return (data_ >> 6) & 0x3f; }

    Piece   cap6()          const { return Piece  ((data_ >> 13) & 0x07); }
    Piece   cap9()          const { return Piece  ((data_ >> 13) & 0x0f); }
    Piece12 captured()      const { return Piece12((data_ >> 12) & 0x0f); }
    
    Piece   promo()         const { return Piece   ((data_ >> 20) & 0x07); }
    Piece12 promo(Side sd)  const { return Piece12(((data_ >> 19) & 0x0e) | sd); }

    bool is_capture()       const { return (data_ & NonCapFlag) == 0; }
    bool is_tactical()      const { return (data_ ^ NonCapFlag) & TacticalFlags; }
    bool is_promo()         const { return data_ & PromoFlags; }
    bool is_under_promo()   const { return data_ & PromoUFlags; }
    bool is_queen_promo()   const { return data_ & PromoQFlag; }
    bool is_ep()            const { return data_ & EPFlag; }
    bool is_single()        const { return data_ & SingleFlag; }
    bool is_double()        const { return data_ & DoubleFlag; }
    bool is_castle()        const { return data_ & CastleFlag; }
    bool is_valid()         const { return data_ & 0xfff; }
    bool is_irrev()         const { return (data_ ^ NonCapFlag) & IrrevFlag; }
    bool is_special()       const { return data_ & SpecialFlags; }

    int index(      )       const { return  data_ & 0xfff; }
    int index(int sd)       const { return (data_ & 0xfff) | (sd << 12); }

    std::string str() const
    {
        std::ostringstream oss;

        oss << square::sq_to_san(orig());
        oss << square::sq_to_san(dest());

        if (promo() == Knight) oss << 'n';
        if (promo() == Bishop) oss << 'b';
        if (promo() == Rook  ) oss << 'r';
        if (promo() == Queen ) oss << 'q';

        return oss.str();
    }

    // Only sets orig, dest, and promotion piece!

    static Move from_string(std::string s)
    {
        int ofile = s[0] - 'a';
        int orank = s[1] - '1';
        int dfile = s[2] - 'a';
        int drank = s[3] - '1';

        int orig = to_sq(ofile, orank);
        int dest = to_sq(dfile, drank);

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
    u64 pinned;
    u64 pinners;
    u64 checkers;
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
