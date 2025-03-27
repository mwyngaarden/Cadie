#ifndef LIST_H
#define LIST_H

#include <iostream>
#include <string>
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
        std::swap(data_[i], data_[j]);
    }

    void add(T& value)
    {
        data_[size_++] = value;
    }

    void add(const T& value)
    {
        data_[size_++] = value;
    }

    void add(T&& value)
    {
        data_[size_++] = std::move(value);
    }
    
    void insert(std::size_t pos, T value)
    {
        ++size_;

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

        for (std::size_t i = pos; i < size_; i++)
            data_[i] = data_[i + 1];

        --size_;
    }
    
    void remove_at(std::size_t pos)
    {
        data_[pos] = data_[size_ - 1];

        --size_;
    }

    void replace(T old_value, T new_value)
    {
        T* p = data_;

        while (*p != old_value) ++p;

        *p = new_value;
    }

    void pop_back()
    {
        --size_;
    }

    T front() const
    {
        return data_[0];
    }

    T back(std::size_t i = 0) const
    {
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
        return data_[i];
    }

    const T& operator[](std::size_t i) const
    {
        return data_[i];
    }

private:
    T data_[N];
    std::size_t size_ = 0;
};

#endif
