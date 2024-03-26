#ifndef LIST_H
#define LIST_H

#include <iostream>
#include <string>
#include <cassert>
#include <cstdint>

template <typename T, std::size_t N>
class List {
public:
    static constexpr std::size_t npos = -1;
    static constexpr std::size_t capacity() { return N; }
    static constexpr std::size_t max_size() { return N; }

    std::size_t size() const { return size_; }

    bool empty() const { return size_ == 0; }

    void clear() { size_ = 0; }

    void swap(std::size_t i, std::size_t j)
    {
        assert(i != j && i < size_ && j < size_);

        std::swap(data_[i], data_[j]);
    }

    void add(T& value)
    {
        assert(size_ < N);

        data_[size_++] = value;
    }

    void add(const T& value)
    {
        assert(size_ < N);

        data_[size_++] = value;
    }

    void add(T&& value)
    {
        assert(size_ < N);

        data_[size_++] = std::move(value);
    }
    
    void insert(std::size_t pos, T value)
    {
        assert(size_ < N);

        ++size_;

        assert(pos <= size_);

        for (std::size_t i = size_; i > pos; i--)
            data_[i] = data_[i - 1];

        data_[pos] = value;
    }

    std::size_t find(T value) const
    {
        for (std::size_t i = 0; i < size_; i++)
            if (data_[i] == value)
                return i;

        return npos;
    }

    // preserves order
    
    void remove(T value)
    {
        std::size_t pos = find(value);

        assert(pos != npos);
        
        for (std::size_t i = pos; i < size_; i++)
            data_[i] = data_[i + 1];

        --size_;
    }
    
    void remove_at(std::size_t pos)
    {
        assert(pos < size_);

        data_[pos] = data_[size_ - 1];

        --size_;
    }

    void replace(T old_value, T new_value)
    {
        assert(find(old_value) != npos);
        assert(find(new_value) == npos);

        T* p = data_;

        while (*p != old_value) ++p;

        *p = new_value;
    }

    void pop_back()
    {
        assert(size_ > 0);

        --size_;
    }

    T front() const
    {
        assert(size_ > 0);

        return data_[0];
    }

    T back(std::size_t i = 0) const
    {
        assert(size_ > 0 && i < size_);

        return data_[size_ - 1 - i];
    }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }

    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

    bool operator==(const T& rhs) const
    {
        if (size_ != rhs.size()) return false;

        for (std::size_t i = 0; i < size_; i++)
            if (data_[i] != rhs[i])
                return false;

        return true;
    }

    T& operator[](std::size_t i)
    {
        assert(i < size_);

        return data_[i];
    }

    const T& operator[](std::size_t i) const
    {
        assert(i < size_);

        return data_[i];
    }

private:
    T data_[N];
    std::size_t size_ = 0;
};

#endif
