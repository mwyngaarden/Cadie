#ifndef HTABLE_H
#define HTABLE_H

#ifdef _MSC_VER
#include <emmintrin.h>
#endif

#include <bit>
#include <vector>
#include <cassert>
#include <cstdint>

template <std::size_t KBytes, class HashEntry>
class HashTable {
public:
    static constexpr std::size_t TableKBytes    = KBytes;
    static constexpr std::size_t EntryBytes     = sizeof(HashEntry);

    HashTable()
    {
        count_ = TableKBytes * 1024 / EntryBytes;
        
        while (std::popcount(count_) > 1)
            count_ &= count_ - 1;

        assert(std::popcount(count_) == 1);

        mask_ = count_ - 1;

        entries_.resize(count_);
    }

    void reset()
    {
        hits_       = 0;
        reads_      = 0;
        used_       = 0;

        memset((void *)entries_.data(), 0, entries_.size() * sizeof(HashEntry));
    }

    std::size_t permille() const
    {
        std::size_t count = 0;

        for (std::size_t i = 0; i < 1000; i++)
            count += !!entries_[i].key;

        return count;
    }

    bool get(uint64_t key, HashEntry& he)
    {
        if (key == 0) return false; // HACK

        reads_++;

        HashEntry& candidate = entries_[key & mask_];

        he.key = key >> (64 - HashEntry::KeyBits);

        if (he.key == candidate.key) {
            hits_++;

            he = candidate;

            return true;
        }

        return false;
    }
    
    void set(uint64_t key, HashEntry& he)
    {
        if (key == 0) return; // HACK

        he.key = key >> (64 - HashEntry::KeyBits);

        entries_[key & mask_] = he;
    }

    void prefetch(uint64_t key)
    {
#ifdef _MSC_VER
        _mm_prefetch((char*)&entries_[key & mask_], _MM_HINT_T0);
#else
        __builtin_prefetch(&entries_[key & mask_]);
#endif
    }

private:

    std::vector<HashEntry> entries_;

    uint64_t hits_       = 0;
    uint64_t reads_      = 0;
    uint64_t used_       = 0;

    uint64_t count_;
    uint64_t mask_;
};

#endif
