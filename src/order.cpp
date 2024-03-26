#include <algorithm>
#include <iostream>
#include <limits>
#include "history.h"
#include "move.h"
#include "order.h"
#include "search.h"
#include "see.h"
using namespace std;

Order::Order(const Node& node, History& history)
{
    bool tactical = node.depth < 0 && !node.pos.checkers();

    GenMode mode = tactical ? GenMode::Tactical : GenMode::Pseudo;

    MoveList moves;

    gen_moves(moves, node.pos, mode);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int see = 2; // 2 = not calculated
        int score;

        if (m == node.entry.move)
            score = ScoreTT;

        else if (m.is_tactical()) {
            score = ScoreTactical;
            score += node.pos.mvv_lva(m);
            
            if (!node.pos.move_is_safe(m, see))
                score -= tactical ? 1024 : ScoreTT;

            if (!tactical && m.is_capture())
                score += history.capture_score(node.pos, m) / 16;
        }

        else if (int index = history.special_index(node.pos, node.ply, m); index)
            score = ScoreSpecial + index;

        else
            score = history.quiet_score(node.pos, m);

        omoves_[count_++] = OMove::make(m, see, score);
    }
}

Move Order::next()
{
    if (index_ >= count_)
        return MoveNone;

    u64 move_max = 0;
    size_t index_max = index_;

    for (size_t i = index_; i < count_; i++) {
        if (OMove::score(omoves_[i]) > OMove::score(move_max)) {
            move_max = omoves_[i];
            index_max = i;
        }
    }

    if (index_max != index_)
        swap(omoves_[index_max], omoves_[index_]);

    score_ = OMove::score(move_max);
    see_ = OMove::see(move_max);
    move_ = OMove::move(move_max);

    index_++;

    return move_;
}

bool Order::singular() const
{
    return count_ == 1;
}

bool Order::special(int score)
{
    return score >= ScoreSpecial && score < ScoreTactical;
}

int Order::score() const
{
    return score_;
}

int& Order::see(const Position& pos, bool calc)
{
    if (calc && see_ == 2) // 2 = not calculated
        see_ = see::calc(pos, move_);

    return see_;
}
