#ifndef HT_H
#define HT_H

#include <bit>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "mem.h"

template <std::size_t Bytes, class T>
class HashTable {
public:
    static constexpr std::size_t Count = Bytes / sizeof(T);
    static constexpr std::size_t Mask  = Count - 1;
    
    static_assert(std::has_single_bit(Bytes));
    static_assert(std::has_single_bit(Count));

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
            count += entries_[i].lock != 0;

        return count;
    }

    bool get(uint64_t key, T& dst)
    {
        assert(key);

        gets_++;

        T& src = entries_[key & Mask];

        dst.lock = key >> (64 - T::LockBits);

        if (dst.lock == src.lock) {
            hits_++;

            dst = src;

            return true;
        }

        return false;
    }
    
    void set(uint64_t key, T& src)
    {
        assert(key);

        src.lock = key >> (64 - T::LockBits);

        entries_[key & Mask] = src;
    }

    void prefetch(uint64_t key)
    {
        mem::prefetch(&entries_[key & Mask]);
    }

    std::size_t hitrate() const { return gets_ ? 1000 * hits_ / gets_ : 0; }

private:
    std::vector<T> entries_;

    std::size_t hits_ = 0;
    std::size_t gets_ = 0;
};

#endif
