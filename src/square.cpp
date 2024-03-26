#include <sstream>
#include <string>
#include <cassert>
#include <cstdint>
#include "square.h"
using namespace std;


namespace square {

    int san_to_sq(string s)
    {
        assert(s.size() == 2);

        int file = s[0] - 'a';
        int rank = s[1] - '1';

        assert(file_is_ok(file));
        assert(rank_is_ok(rank));

        int sq = to_sq64(file, rank);

        assert(sq64_is_ok(sq));

        return sq;
    }

    string sq_to_san(int sq)
    {
        assert(sq64_is_ok(sq));

        int file = square::file(sq);
        int rank = square::rank(sq);

        assert(file_is_ok(file));
        assert(rank_is_ok(rank));

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

    int dist_edge(int sq)
    {
        int f = file(sq);

        return f <= FileD ? f : FileH - f;
    }

}
