#ifndef TT_H
#define TT_H

#include <algorithm>
#include <bit>
#include <vector>
#include <cstring>
#include <omp.h>
#include "eval.h"
#include "mem.h"
#include "misc.h"
#include "move.h"


constexpr u8 BoundNone  = 0;
constexpr u8 BoundLower = 1;
constexpr u8 BoundUpper = 2;
constexpr u8 BoundExact = 3;

constexpr std::size_t TTSizeMBMin     =          1; //   1 MB
constexpr std::size_t TTSizeMBDefault =         16; //  16 MB
constexpr std::size_t TTSizeMBMax     = 128 * 1024; // 128 GB

int score_to_tt(int score, int ply);
int score_from_tt(int score, int ply);

struct Entry {
    using Lock = u32;

    static constexpr std::size_t LockBits = sizeof(Lock) * 8;

    Lock lock;
    Move move;
    i16 score;
    i16 eval;
    i8 depth;
    u8 gen;
    u8 bound;
    u8 pad;
    
    static Lock calc_lock(u64 key)
    {
        return static_cast<Lock>(key >> (64 - LockBits));
    }

    bool is_valid() const
    {
        if (move != MoveNone && !move.is_valid())
            return false;
        if (abs(score) > ScoreMate)
            return false;
        if (depth > DepthMax)
            return false;
        if (bound == 0 || (bound & ~BoundExact) != 0)
            return false;

        return true;
    }

    void clear()
    {
        lock    = 0;
        move    = MoveNone;
        score   = -ScoreMate;
        eval    = -ScoreMate;
        depth   = 0;
        gen     = 0;
        bound   = BoundNone;
    }
};

static_assert(sizeof(Entry) == 16);
static_assert(std::has_single_bit(sizeof(Entry)));

class TT {
public:

    TT(std::size_t mb);

    void init();

    bool get(Entry& dst, u64 key, int ply);
    void set(Entry& src, u64 key);

    std::size_t count() const { return count_; }

    void reset()
    {
        gen_  = 0;
        hits_ = 0;
        gets_ = 0;
        sets_ = 0;

        std::memset((void *)entries_.data(), 0, size_);
    }

    void age()
    {
        gen_++;
    }

    std::size_t permille() const
    {
        std::size_t pm = 0;

        for (std::size_t i = 0; i < 1000; i++) {
            const Entry& e = entries_[i];

            pm += e.depth && e.gen == gen_;
        }

        return pm;
    }

    void prefetch(u64 key)
    {
        mem::prefetch(&entries_[key & mask_]);
    }

    std::size_t size() const { return size_; }
    std::size_t size_mb() const { return size_ / 1024 / 1024; }

    std::size_t hp() const { return gets_ ? 100 * hits_ / gets_ : 0; }

private:
    std::vector<Entry> entries_;

    u64 count_;
    u64 mask_;

    u8  gen_  = 0;
    u64 hits_ = 0;
    u64 gets_ = 0;
    u64 sets_ = 0;
    
    std::size_t size_;
};

extern TT ttable;

#endif
