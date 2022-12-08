#ifndef HTABLE_H
#define HTABLE_H

#include <bit>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "mem.h"

template <std::size_t K, class T>
class HashTable {
public:
    static constexpr std::size_t Bytes = K * 1024;
    static constexpr std::size_t Count = Bytes / sizeof(T);
    static constexpr std::size_t Mask  = Count - 1;
    
    static_assert(std::popcount(Bytes) == 1);
    static_assert(std::popcount(Count) == 1);

    HashTable()
    {
        entries_.resize(Count);
    }

    void reset()
    {
        std::memset((void *)entries_.data(), 0, Bytes);
    }

    std::size_t permille() const
    {
        std::size_t count = 0;

        for (std::size_t i = 0; i < 1000; i++)
            count += !!entries_[i].key;

        return count;
    }

    bool get(uint64_t key, T& e)
    {
        assert(key != 0);

        T& candidate = entries_[key & Mask];

        e.key = key >> (64 - T::KeyBits);

        if (e.key == candidate.key) {
            e = candidate;

            return true;
        }

        return false;
    }
    
    void set(uint64_t key, T& e)
    {
        assert(key != 0);

        if (key == 0) return; // HACK

        e.key = key >> (64 - T::KeyBits);

        entries_[key & Mask] = e;
    }

    void prefetch(uint64_t key)
    {
        my_prefetch(&entries_[key & Mask]);
    }

private:
    std::vector<T> entries_;
};

#endif
