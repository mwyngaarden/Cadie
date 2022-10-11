#include "string.h"
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

Tokenizer::Tokenizer(const string& s, char delim)
{
    stringstream ss(s);
    string token;

    while (getline(ss, token, delim))
        tokens_.push_back(token);
}

size_t Tokenizer::size() const
{
    return tokens_.size();
}

const string& Tokenizer::operator[](size_t i) const
{
    return tokens_[i];
}
