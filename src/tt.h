#ifndef TT_H
#define TT_H

#ifdef _MSC_VER
#include <emmintrin.h>
#endif

#include <algorithm>
#include <vector>
#include <cstring>
#include "eval.h"
#include "move.h"
#include "search.h"
#include "types.h"


constexpr int BoundLower    = 0;
constexpr int BoundExact    = 1;
constexpr int BoundUpper    = 2;

int score_to_tt(int score, int ply);
int score_from_tt(int score, int ply);

class TransTable {
public:
    using Key = u32;

    static constexpr int KeyBits = sizeof(Key) * 8;
    static constexpr int ClusterCount = 4;

    struct Entry {
        Key key;
        Move move;
        i16 score;
        i16 eval;
        i8 depth;
        u8 gen : 6;
        u8 bound : 2;
        u16 pad;
    };

    static_assert(sizeof(Entry) == 16);

    struct Cluster { Entry entries[ClusterCount]; };

    static_assert(sizeof(Cluster) == 64);

    TransTable(std::size_t size_mb);

    bool get(u64 key, int ply, Move& move, i16& eval, i16& score, i8& depth, u8& bound);
    void set(u64 key, int ply, Move& move, i16  eval, i16  score, i8  depth, u8  bound);

    std::size_t count() const { return count_; }

    void reset_stats()
    {
        hits = 0;
        reads = 0;
    }

    void reset()
    {
        gen_ = 0;
        
        std::memset((void *)clusters_.data(), 0, clusters_.size() * sizeof(Cluster));

        reset_stats();
    }

    void inc_gen()
    {
        gen_ = (gen_ + 1) & 63;
    }

    std::size_t permille() const
    {
        std::size_t pm = 0;

        for (std::size_t i = 0; i < 1000; i++) {
            const Cluster& c = clusters_[i];

            for (std::size_t j = 0; j < ClusterCount; j++) {
                const Entry& e = c.entries[j];

                pm += e.bound && e.gen == gen_;
            }
        }

        return pm / ClusterCount;
    }

    static Key mod_key(u64 key)
    {
        return static_cast<Key>(key >> (64 - KeyBits));
    }

    void prefetch(u64 key) const
    {
#ifdef _MSC_VER
        _mm_prefetch((char*)&clusters_[key & mask_], _MM_HINT_T0);
#else
        __builtin_prefetch(&clusters_[key & mask_]);
#endif
    }

    std::size_t size_mb() const { return size_mb_; }

    u64 hits;
    u64 reads;

private:
    std::vector<Cluster> clusters_;

    u64 count_;
    u64 mask_;

    u8 gen_;

    std::size_t size_mb_;
};

extern TransTable thtable;

#endif
