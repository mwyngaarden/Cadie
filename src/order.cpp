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
    bool qs = node.depth < 0 && !node.pos.checkers();

    MoveList moves;

    gen_moves(moves, node.pos, qs ? GenMode::Tactical : GenMode::Pseudo);

    Move killer1;
    Move killer2;
    Move counter;

    history.specials(node, killer1, killer2, counter);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int see = 2; // 2 = not calculated
        int score;

        if (m == node.entry.move)
            score = ScoreTT;

        else if (m.is_tactical()) {
            score = ScoreTactical;
            score += node.pos.mvv_lva(m);

            if ((see = see::calc(node.pos, m)) == 0)
                score -= qs ? 1024 : ScoreTT;
        }

        else if (m == killer1) score = ScoreKiller1;
        else if (m == killer2) score = ScoreKiller2;
        else if (m == counter) score = ScoreCounter;

        else {
            assert(!m.is_tactical());

            score = history.score(node.pos, m);
        }

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

bool Order::is_special(int score)
{
    return score >= ScoreCounter && score <= ScoreKiller1;
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
