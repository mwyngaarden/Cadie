#ifndef TYPES_H
#define TYPES_H

#include <bitset>
#include <cassert>
#include <cstdint>

constexpr bool DebugLog = false;

#ifndef NDEBUG
constexpr bool Debug = true;
#else
constexpr bool Debug = false;
#endif

using BitSet = std::bitset<128>;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// TODO tune
constexpr int DepthMin =  -4;
constexpr int DepthMax = 127;
constexpr int PliesMax = DepthMax - DepthMin + 2;
constexpr int MovesMax = 128;

// phase

constexpr int PhaseMg       = 0;
constexpr int PhaseEg       = 1;
constexpr int PhaseCount    = 2;

constexpr int PhaseKnight   = 1;
constexpr int PhaseBishop   = 1;
constexpr int PhaseRook     = 2;
constexpr int PhaseQueen    = 4;
constexpr int PhaseMax      = 4 * PhaseKnight + 4 * PhaseBishop + 4 * PhaseRook + 2 * PhaseQueen;

constexpr bool phase_is_ok(int phase) { return phase == PhaseMg || phase == PhaseEg; }

struct Value {
    int mg = 0;
    int eg = 0;

    bool value_is_ok() const
    {
        // 30000 is completely arbitrary
        
        return abs(mg) < 30000 && abs(eg) < 30000;
    }

    constexpr int operator[](int phase) const
    {
        assert(phase_is_ok(phase));

        return phase == PhaseMg ? mg : eg;
    }

    Value& operator+=(const Value& rhs)
    {
        mg += rhs.mg;
        eg += rhs.eg;

        return *this;
    }
    
    Value& operator+=(int x)
    {
        mg += x;
        eg += x;

        return *this;
    }

    Value& operator-=(const Value& rhs)
    {
        mg -= rhs.mg;
        eg -= rhs.eg;

        return *this;
    }
    
    Value& operator*=(int x)
    {
        mg *= x;
        eg *= x;
        
        return *this;
    }
    
    Value& operator/=(int x)
    {
        mg /= x;
        eg /= x;
        
        return *this;
    }

    int lerp(int phase, int factor = 128) const
    {
        assert(value_is_ok());

        return ((mg * (PhaseMax - phase)) + (eg * phase) * factor / 128) / PhaseMax;
    }
};

inline Value operator+(Value lhs, const Value& rhs)
{
    lhs += rhs;
    return lhs;
}

inline Value operator-(Value lhs, const Value& rhs)
{
    lhs -= rhs;
    return lhs;
}

inline Value operator*(int x, Value rhs)
{
    rhs *= x;
    return rhs;
}

inline Value operator*(Value lhs, int x)
{
    lhs.mg *= x;
    lhs.eg *= x;
    return lhs;
}

inline Value operator/(Value lhs, int x)
{
    lhs.mg /= x;
    lhs.eg /= x;
    return lhs;
}

#endif
