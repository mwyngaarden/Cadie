#ifndef MISC_H
#define MISC_H

#include <cassert>
#include <cstdint>
#include "list.h"

constexpr char CADIE_VERSION[] = "2.0";
constexpr char CADIE_DATE[] = __DATE__;
constexpr char CADIE_TIME[] = __TIME__;

#define PROFILE_NONE 0
#define PROFILE_SOME 1
#define PROFILE_ALL  2

#define PROFILE PROFILE_NONE

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

using KeyStack = List<u64, 1024>;

constexpr int DepthMin =  -4;
constexpr int DepthMax = 120;
constexpr int PliesMax = DepthMax - DepthMin + 1;
constexpr int MovesMax = 224;

constexpr bool ply_is_ok(int ply) { return ply >= 0 && ply < PliesMax; }

constexpr int PhaseMg       = 0;
constexpr int PhaseEg       = 1;
constexpr int PhaseCount    = 2;

constexpr int PhaseN        = 1;
constexpr int PhaseB        = 1;
constexpr int PhaseR        = 2;
constexpr int PhaseQ        = 4;
constexpr int PhaseMax      = 4 * PhaseN + 4 * PhaseB + 4 * PhaseR + 2 * PhaseQ;

constexpr bool phase_is_ok(int phase) { return phase == PhaseMg || phase == PhaseEg; }

struct Value {
    int mg;
    int eg;

    constexpr Value(int m, int e) : mg(m), eg(e) { }

    constexpr Value() : Value(0, 0) { }

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
        return (mg * (PhaseMax - phase) + (eg * phase * factor / 128)) / PhaseMax;
    }

    friend Value operator-(Value val)
    {
        return Value(-val.mg, -val.eg);
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
