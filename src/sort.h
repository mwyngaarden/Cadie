#include <limits>
#include "move.h"
#include "search.h"

struct Sort {
    static constexpr int ScoreMin       = std::numeric_limits<int>::min();
    static constexpr int ScoreTT        = std::numeric_limits<int>::max();
    static constexpr int ScoreCapBase   = 65536;
    static constexpr int ScoreKiller1   = ScoreCapBase - 1;
    static constexpr int ScoreKiller2   = ScoreCapBase - 2;

    Sort(const Node& node) : node_(node)
    {
        GenMode mode = node_.depth <= 0 && !node_.in_check ? GenMode::Tactical : GenMode::Pseudo;

        gen_moves(moves_, node_.pos, mode);

        for (std::size_t i = 0; i < moves_.size(); i++) {
            Move m = moves_[i];

            if (m == node_.tt_move)
                scores_[i] = ScoreTT;
            else if (m.is_capture()) {
                u8 mpiece = node_.pos[m.orig()];
                u8 opiece = m.capture_piece();

                int mptype = P256ToP6[mpiece];
                int optype = P256ToP6[opiece];

                int opvalue = ValuePiece[optype][PhaseMg];

                int mvv_lva_order = opvalue - mptype;

                scores_[i] = ScoreCapBase + mvv_lva_order;
            }
            else if (m == Killers[node_.side][node_.ply][0])
                scores_[i] = ScoreKiller1;
            else if (m == Killers[node_.side][node_.ply][1])
                scores_[i] = ScoreKiller2;
            else
                scores_[i] = History[node_.side][m.orig()][m.dest()];
        }
    }

    Move next(int& sort_score)
    {
        if (index_ >= moves_.size())
            return MoveNone;

        int score_max = ScoreMin;
        std::size_t index_max = index_;

        for (std::size_t i = index_; i < moves_.size(); i++) {
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
    std::size_t index_ = 0;
    int scores_[MoveList::max_size()];
    const Node& node_;
};
