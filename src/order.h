#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <limits>
#include "move.h"
#include "search.h"
#include "see.h"

struct Order {
    static constexpr int ScoreTT        =  1 << 20;
    static constexpr int ScoreTactical  =  1 << 19;
    static constexpr int ScoreSpecial   =  1 << 18;

    Order(const Node& node)
    {
        bool tactical = node.depth < 0 && !node.pos.checkers();

        GenMode mode = tactical ? GenMode::Tactical : GenMode::Pseudo;

        MoveList moves;

        gen_moves(moves, node.pos, mode);

        for (std::size_t i = 0; i < moves.size(); i++) {
            Move m = moves[i];

            int see = ScoreNone;
            int score;

            if (m == node.entry.move)
                score = ScoreTT - 1;

            else if (m.is_tactical()) {
                score = ScoreTactical;
                score += node.pos.mvv_lva(m);
                
                if (!node.pos.move_is_safe(m, see))
                    score -= ScoreTT;
            }

            else if (int index = history.is_special(node.pos, node.ply, m); index)
                score = ScoreSpecial + index;

            else
                score = history.score(node.pos, m);

            smoves_.add(m, score, see);
        }
    }

    Move next()
    {
        if (index_ >= smoves_.size())
            return MoveNone;

        auto score_max = -ScoreTT - 1;
        auto index_max = index_;

        for (std::size_t i = index_; i < smoves_.size(); i++) {
            if (smoves_[i].score > score_max) {
                score_max = smoves_[i].score;
                index_max = i;
            }
        }

        if (index_max != index_)
            smoves_.swap(index_max, index_);

        score_ = smoves_[index_].score;
        see_ = smoves_[index_].see;
        move_ = smoves_[index_].move;

        index_++;

        return move_;
    }

    bool is_singular() const
    {
        return smoves_.size() == 1;
    }

    static bool is_special(int score)
    {
        return score >= ScoreSpecial && score < ScoreTactical;
    }

    int score() const
    {
        return score_;
    }

    int& see(const Position& pos, bool calc)
    {
        if (calc && see_ == ScoreNone)
            see_ = see_move(pos, move_);

        return see_;
    }

private:
    std::size_t index_ = 0;
    MoveExtList smoves_;

    Move move_;
    int see_;
    int score_;
};

#endif
