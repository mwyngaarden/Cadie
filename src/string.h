#ifndef STRING_H
#define STRING_H

#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

class Tokenizer {
public:
    Tokenizer(const std::string& s, char delim = ' ')
    {
        std::stringstream ss(s);
        std::string token;

        while (std::getline(ss, token, delim))
            tokens_.push_back(token);
    }
    
    Tokenizer(int argc, char* argv[])
    {
        for (int i = 0; i < argc; i++)
            tokens_.push_back(argv[i]);
    }

    std::string get(std::size_t i) const
    {
        return tokens_[i];
    }
    
    std::string get(std::size_t i, std::string def) const
    {
        return i < tokens_.size() ? tokens_[i] : def;
    }

    std::size_t size() const 
    {
        return tokens_.size();
    }

    const std::string& operator[](std::size_t i) const
    {
        return tokens_[i];
    }

    std::vector<std::string> subtok(std::size_t i) const
    {
        assert(i < tokens_.size());
        return std::vector<std::string>(tokens_.begin() + i, tokens_.end());
    }

private:
    std::vector<std::string> tokens_;
};

#endif
