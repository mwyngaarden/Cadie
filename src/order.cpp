#include <algorithm>
#include <iostream>
#include <limits>
#include "history.h"
#include "move.h"
#include "order.h"
#include "search.h"
using namespace std;

Order::Order(Position& pos, const History& history, Move best_move, int ply, int depth) : pos_(pos)
{
    bool tactical = depth <= -1 && !pos.checkers();
    bool prune    = depth <=  0 && !pos.checkers();
    bool checks   = depth ==  0 && !pos.checkers();

    MoveList moves;

    if (checks) {
        gen_moves(moves, pos, GenMode::Tactical);
        add_checks(moves, pos);
    }
    else
        gen_moves(moves, pos, tactical ? GenMode::Tactical : GenMode::Pseudo);

    Move killer1 = Move::None();
    Move killer2 = Move::None();
    Move counter = Move::None();

    if (!tactical)
        history.specials(pos, ply, killer1, killer2, counter);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        int score = 0;
        int see = -1;

        if (prune && !(see = pos.see(m)))
            continue;

        if (m == best_move)
            score = ScoreTT;

        else if (m.is_tactical()) {
            score = ScoreTactical + pos.mvv_lva(m);

            if (see == -1)
                see = pos.see(m);

            if (!see)
                score -= ScoreTT;
        }

        else if (!tactical) {
            if (m == killer1)
                score = ScoreKiller1;
            else if (m == killer2)
                score = ScoreKiller2;
            else if (m == counter)
                score = ScoreCounter;
            else
                score = history.score(pos, m);
        }

        emoves_[count_++] = ExtMove::make(m, score);
    }
}

Order::Order(Position& pos, Move best_move) : pos_(pos)
{
    MoveList moves;

    gen_moves(moves, pos, GenMode::Tactical);

    for (size_t i = 0; i < moves.size(); i++) {
        Move m = moves[i];

        if (!pos.see(m))
            continue;

        int score = m == best_move ? ScoreTT : ScoreTactical + pos.mvv_lva(m);

        emoves_[count_++] = ExtMove::make(m, score);
    }
}

Move Order::next()
{
    if (index_ >= count_)
        return Move::None();

    u64 move_max = 0;
    size_t index_max = index_;

    for (size_t i = index_; i < count_; i++) {
        if (ExtMove::score(emoves_[i]) > ExtMove::score(move_max)) {
            move_max = emoves_[i];
            index_max = i;
        }
    }

    if (index_max != index_)
        swap(emoves_[index_max], emoves_[index_]);

    score_ = ExtMove::score(move_max);
    move_ = ExtMove::move(move_max);

    see_ = score_ == ScoreTT || !move_.is_tactical() ? 2
         : score_ >= ScoreTactical ? 1 : 0;

    index_++;

    return move_;
}

bool Order::singular() const
{
    return count_ == 1;
}

int Order::score() const
{
    return score_;
}

int Order::see()
{
    if (see_ == 2)
        see_ = pos_.see(move_);

    return see_;
}
