#ifndef TT_H
#define TT_H

#include <algorithm>
#include <vector>
#include <cstring>
#include "eval.h"
#include "mem.h"
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
        i16 depth;
        u16 gb;

        u32 get_gen  () const { return (gb >> 2) & 0x3ful; }
        u32 get_bound() const { return  gb       & 0x03ul; }

        void set_gen  (u32 gen  ) { gb = (gb & (~0xfcul)) | ((gen   & 0x0000003ful) << 2); }
        void set_bound(u32 bound) { gb = (gb & (~0x03ul)) |  (bound & 0x00000003ul)      ; }
    };

    static_assert(sizeof(Entry) == 16);

    struct Cluster { Entry entries[ClusterCount]; };

    static_assert(sizeof(Cluster) == 64);

    TransTable(std::size_t mb);

    void init();

    bool get(u64 key, int ply, Move& move, i16& eval, i16& score, i8& depth, u8& b);
    void set(u64 key, int ply, Move& move, i16  eval, i16  score, i8  depth, u8  b);

    std::size_t count() const { return count_; }

    void reset()
    {
        gen_ = 0;
        
        std::memset((void *)clusters_.data(), 0, size_);
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

                pm += e.get_bound() && e.get_gen() == gen_;
            }
        }

        return pm / ClusterCount;
    }

    static Key mod_key(u64 key)
    {
        return static_cast<Key>(key >> (64 - KeyBits));
    }

    void prefetch(u64 key)
    {
        my_prefetch(&clusters_[key & mask_]);
    }

    std::size_t size() const { return size_; }
    std::size_t size_mb() const { return size_ / 1024 / 1024; }


private:
    std::vector<Cluster> clusters_;

    u64 count_;
    u64 mask_;

    u8 gen_ = 0;

    std::size_t size_;
};

extern TransTable thtable;

#endif
