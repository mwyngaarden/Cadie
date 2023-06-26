#ifndef HISTORY_H
#define HISTORY_H

#include <atomic>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "pos.h"
#include "timer.h"
#include "tt.h"
#include "uci.h"
#include "uciopt.h"


class History {
public:
    static constexpr int HistoryMax = DepthMax * DepthMax;

    void clear()
    {
        std::memset(counters_, 0, sizeof(counters_));
        std::memset(killers_, 0, sizeof(killers_));
        std::memset(quiets_, 0, sizeof(quiets_));
    }

    int is_special(const Position& pos, int ply, const Move& m) const
    {
        assert(ply_is_ok(ply));

        side_t side = pos.side();

        if (m == killers_[side][ply][0]) return 3;
        if (m == killers_[side][ply][1]) return 2;

        if (Move pm = pos.pm(); pm.is_valid()) {
            int p6 = pos.square<6>(pm.dest());
            assert(piece_is_ok(p6));

            if (m == counters_[p6][pm.dest()])
                return 1;
        }

        return 0;
    }
    
    void add_killer(side_t side, int ply, const Move& m)
    {
        assert(side_is_ok(side));
        assert(ply_is_ok(ply));

        if (killers_[side][ply][0] != m) {
            killers_[side][ply][1] = killers_[side][ply][0];
            killers_[side][ply][0] = m;
        }
    }
    
    void rem_killers(side_t side, int ply)
    {
        assert(side_is_ok(side));
        assert(ply_is_ok(ply));

        killers_[side][ply][0] = MoveNone;
        killers_[side][ply][1] = MoveNone;
    }
    
    void add_counter(const Position& pos, const Move& m)
    {
        if (Move pm = pos.pm(); pm.is_valid()) {
            int p6 = pos.square<6>(pm.dest());
            assert(piece_is_ok(p6));

            counters_[p6][pm.dest()] = m;
        }
    }

    void inc(const Position& pos, int depth, const Move& bm)
    {
        assert(bm.is_valid());
        assert(!bm.is_tactical());

        int bonus = depth * depth;

        update(p_quiet(pos, bm), bonus);
    }
    
    void dec(const Position& pos, int depth, const Move& bm, const MoveList& moves)
    {
        assert(bm.is_valid());
        assert(!bm.is_tactical());

        int bonus = -1 - depth * depth / 2;
       
        for (const Move& m : moves) {
            if (m == bm) continue;

            update(p_quiet(pos, m), bonus);
        }
    }

    int score(const Position& pos, const Move& m)
    {
        int s = *p_quiet(pos, m);

        return s;
    }

private:
    
    i16 * p_quiet(const Position& pos, const Move& m)
    {
        return &quiets_[pos.side()][m.orig()][m.dest()];
    }

    void update(i16 * p, int bonus)
    {
        int score = *p;

        score += 32 * bonus - score * std::abs(bonus) / 128;
        score  = std::clamp(score, -HistoryMax, HistoryMax);

        *p = score;
    }


    i16 quiets_[SideCount][SquareCount][SquareCount];
    Move killers_[SideCount][PliesMax][2];
    Move counters_[PieceCount6][SquareCount];
};

extern History history;

#endif
