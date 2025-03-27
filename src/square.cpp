#include <sstream>
#include <string>
#include <cstdint>
#include "square.h"
using namespace std;


namespace square {

    int san_to_sq(string s)
    {
        int file = s[0] - 'a';
        int rank = s[1] - '1';

        int sq = to_sq(file, rank);

        return sq;
    }

    string sq_to_san(int sq)
    {
        int file = square::file(sq);
        int rank = square::rank(sq);

        ostringstream oss;
        oss << static_cast<char>('a' + file);
        oss << static_cast<char>('1' + rank);

        return oss.str();
    }

    int dist(int sq1, int sq2)
    {
        int r1 = rank(sq1);
        int f1 = file(sq1);
        int r2 = rank(sq2);
        int f2 = file(sq2);

        int rd = std::abs(r1 - r2);
        int fd = std::abs(f1 - f2);

        return std::max(rd, fd);
    }

}
