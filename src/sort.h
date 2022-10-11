#include <limits>
#include "move.h"
#include "search.h"

struct Sort {
    static constexpr int ScoreMin    = std::numeric_limits<int>::lowest();
    static constexpr int ScoreMax    = std::numeric_limits<int>::max();
    static constexpr int ScoreOffset = 65536;

    Sort(const Node& node, 
         const i16 (&history)[2][128][128],
         const Move (&killers)[2][PliesMax][2]) : n_(node)
    {
        const GenType gtype = n_.depth <= 0 && !n_.in_check ? Tactical : Pseudo;

        //i64 time_begin = Util::now();
        gen_moves(moves_, n_.pos, gtype);
        //i64 time_end = Util::now();

        //global_stats.time_gen += time_end - time_begin;

        score_moves(history, killers);
    }

    void score_moves(const i16 (&history)[2][128][128],
                     const Move (&killers)[2][PliesMax][2])
    {
        for (std::size_t i = 0; i < moves_.size(); i++) {
            const Move m = moves_[i];

            if (m == n_.tt_move)
                scores_[i] = ScoreMax;
            else if (m.is_capture()) {
                const u8 mpiece = n_.pos[m.orig()];
                const u8 opiece = m.capture_piece();

                const int mptype = P256ToP6[mpiece];
                const int optype = P256ToP6[opiece];

                const int opvalue = ValuePiece[optype][PhaseMg];

                const int mvv_lva_order = opvalue - mptype;

                scores_[i] = mvv_lva_order + ScoreOffset;
            }
            else if (m == killers[n_.side][n_.ply][0])
                scores_[i] = ScoreOffset - 10000 + 1;
            else if (m == killers[n_.side][n_.ply][1])
                scores_[i] = ScoreOffset - 10000 + 0;
            else {
                const int side = n_.pos.side();
                const int orig = m.orig();
                const int dest = m.dest();

                scores_[i] = history[side][orig][dest];
            }
        }
    }

    Move next(int& sort_score)
    {
        if (index_ >= moves_.size())
            return MoveNone;

        int score_max = ScoreMin;
        size_t index_max = index_;

        for (size_t i = index_; i < moves_.size(); i++) {
            if (scores_[i] > score_max) {
                score_max = scores_[i];
                index_max = i;
            }
        }

        if (index_max != index_) {
            std::swap(moves_[index_], moves_[index_max]);
            std::swap(scores_[index_], scores_[index_max]);
        }

        sort_score = scores_[index_];

        return moves_[index_++];
    }

private:
    MoveList moves_;
    size_t index_ = 0;
    int scores_[MoveList::max_size()];
    const Node& n_;
};
