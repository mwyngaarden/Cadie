#include <sstream>
#include <string>
#include <cassert>
#include <cstdint>
#include "square.h"
using namespace std;

int Distance[128][128];

void square_init()
{
    for (int i = 0; i < 128; i++) {
        if (!sq88_is_ok(i)) continue;

        for (int j = 0; j < 128; j++) {
            if (!sq88_is_ok(j)) continue;

            Distance[i][j] = sq88_dist(i, j);
        }
    }
}

int san_to_sq88(string s)
{
    assert(s.size() == 2);

    int file = s[0] - 'a';
    int rank = s[1] - '1';

    assert(file_is_ok(file));
    assert(rank_is_ok(rank));

    int sq = to_sq88(file, rank);

    assert(sq88_is_ok(sq));

    return sq;
}

string sq88_to_san(int sq)
{
    assert(sq88_is_ok(sq));

    int file = sq88_file(sq);
    int rank = sq88_rank(sq);

    assert(file_is_ok(file));
    assert(rank_is_ok(rank));

    ostringstream oss;
    oss << static_cast<char>('a' + file);
    oss << static_cast<char>('1' + rank);

    return oss.str();
}

int sq88_rank(int sq)
{
    return sq >> 4;
}

int sq88_rank(int sq, int side)
{
    const int rank = sq88_rank(sq);

    return rank ^ (7 * side);
}

bool sq88_edge(int sq)
{
    const int file = sq88_file(sq);

    return file == FileA || file == FileH;
}

int sq88_dist_edge(int sq)
{
    constexpr int rdist[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };
    constexpr int fdist[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

    const int r = sq88_rank(sq);
    const int f = sq88_file(sq);

    return std::min(rdist[r], fdist[f]);
}

int sq88_quadrant(int sq)
{
    const int r = sq88_rank(sq);
    const int f = sq88_file(sq);
    
    if (r >= Rank5 && f <= FileD)
        return 0;
    else if (r >= Rank5 && f >= FileE)
        return 1;
    else if (r <= Rank4 && f <= FileD)
        return 2;
    else
        return 3;
}

int sq88_dist(int sq1, int sq2)
{
    const int r1 = sq88_rank(sq1);
    const int f1 = sq88_file(sq1);
    const int r2 = sq88_rank(sq2);
    const int f2 = sq88_file(sq2);

    int rmin = std::abs(r1 - r2);
    int fmin = std::abs(f1 - f2);

    return std::max(rmin, fmin);
}

int sq88_dist_corner(int sq)
{
    const int q = sq88_quadrant(sq);

    switch (q) {
    case 0: return sq88_dist(sq, A8);
    case 1: return sq88_dist(sq, H8);
    case 2: return sq88_dist(sq, A1);
    case 3: return sq88_dist(sq, H1);
    default:
        assert(false);
        return 0;
    }
}

bool sq88_center16(int sq)
{
    const int rank = sq88_rank(sq);
    const int file = sq88_file(sq);

    return rank >= Rank3 && rank <= Rank6
        && file >= FileC && file <= FileF;
}
