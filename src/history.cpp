#include <limits>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "history.h"
#include "misc.h"
#include "move.h"
#include "pos.h"
#include "search.h"
using namespace std;


void History::clear()
{
    memset(counters_, 0, sizeof(counters_));
    memset(killers_, 0, sizeof(killers_));
    memset(quiets_, 0, sizeof(quiets_));
    memset(captures_, 0, sizeof(captures_));
}

void History::update(const Node& node, const MoveList& qmoves, const MoveList& cmoves)
{
    Move bm = node.bm;

    side_t side = node.pos.side();

    int ply = node.ply;
    int depth = node.depth;

    int inc = depth * depth;
    int dec = -1 - depth * depth / 2;

    if (!bm.is_tactical()) {

        // Counter moves
        
        if (Move mp = node.pos.move_prev(); mp.is_valid()) {
            int type = P256ToP6[node.pos.board(mp.dest64())];

            assert(piece_is_ok(type));

            counters_[type][mp.dest64()] = bm;
        }

        // Killer moves
        
        if (killers_[side][ply][0] != bm) {
            killers_[side][ply][1] = killers_[side][ply][0];
            killers_[side][ply][0] = bm;
        }

        // History heuristic
        
        update(quiet_ptr(node.pos, bm), inc);

        for (const Move& m : qmoves) {
            if (m == bm) continue;

            update(quiet_ptr(node.pos, m), dec);
        }
    }

    else if (bm.is_capture()) {

        // Capture history
        
        update(capture_ptr(node.pos, bm), inc);

    }

    for (const Move& m : cmoves) {
        if (m == bm) continue;

        update(capture_ptr(node.pos, m), dec);
    }
}

int History::special_index(const Position& pos, int ply, const Move& m) const
{
    if (m == killers_[pos.side()][ply][0]) return 3;
    if (m == killers_[pos.side()][ply][1]) return 2;

    if (Move mp = pos.move_prev(); mp.is_valid()) {
        int type = P256ToP6[pos.board(mp.dest64())];

        assert(piece_is_ok(type));

        if (m == counters_[type][mp.dest64()])
            return 1;
    }

    return 0;
}

void History::clear_killers(side_t side, int ply)
{
    assert(side_is_ok(side));
    assert(ply_is_ok(ply));

    killers_[side][ply][0] = MoveNone;
    killers_[side][ply][1] = MoveNone;
}

int History::quiet_score(const Position& pos, const Move& m)
{
    int s = *quiet_ptr(pos, m);

    return s;
}

int History::capture_score(const Position& pos, const Move& m)
{
    int s = *capture_ptr(pos, m);

    return s;
}

i16 * History::quiet_ptr(const Position& pos, const Move& m)
{
    return &quiets_[pos.side()][m.orig64()][m.dest64()];
}

i16 * History::capture_ptr(const Position& pos, const Move& m)
{
    int mtype = P256ToP6[pos.board(m.orig64())];
    int otype = P256ToP6[m.capture_piece()];

    return &captures_[mtype][m.dest64()][otype];
}

void History::update(i16 * p, int bonus)
{
    int score = *p;

    score += 32 * bonus - score * std::abs(bonus) / 128;
    score  = std::clamp(score, -HistoryMax, HistoryMax);

    *p = score;
}
