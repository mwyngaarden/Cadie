#include "square.h"
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
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
