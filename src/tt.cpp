#include <bit>

#include <cassert>
#include <cstdint>
#include "tt.h"

using namespace std;

constexpr size_t TTSizeMB = 16;

TransTable thtable(TTSizeMB);

TransTable::TransTable(size_t mb)
{
    assert(mb >= 1 && mb <= 1024);

    size_ = mb * 1024 * 1024;
    
    while (popcount(size_) > 1) size_ &= size_ - 1;
    assert(popcount(size_) == 1);

    count_ = size_ / sizeof(TransTable::Cluster);
    assert(popcount(count_) == 1);

    mask_ = count_ - 1;
}

void TransTable::init()
{
    assert(clusters_.empty());

    clusters_.resize(count_);

    if (clusters_.capacity() > count_) {
        assert(false);
        clusters_.shrink_to_fit();
    }
}

bool TransTable::get(u64 key, int ply, Move& move, i16& eval, i16& score, i8& depth, u8& bound)
{
    Cluster& c = clusters_[key & mask_];

    key = mod_key(key);

    for (std::size_t i = 0; i < ClusterCount; i++) {
        Entry& e = c.entries[i];

        if (e.key == key) {
            e.set_gen(gen_);

            depth   = e.depth;
            bound   = e.get_bound();
            move    = e.move;
            score   = score_from_tt(e.score, ply);
            eval    = e.eval;

            return true;
        }
    }

    return false;
}

void TransTable::set(u64 key, int ply, Move& move, i16 eval, i16 score, i8 depth, u8 bound)
{
    Cluster& c = clusters_[key & mask_];

    Entry* e = &c.entries[0];

    key = mod_key(key);

    std::size_t i;

    // FIXME
    for (i = 0; i < ClusterCount && c.entries[i].key != key; i++) {
        i8 a =           e->depth - (gen_ -           e->get_gen());
        i8 b = c.entries[i].depth - (gen_ - c.entries[i].get_gen());

        if (a >= b)
            e = &c.entries[i];
    }

    if (i != ClusterCount)
        e = &c.entries[i];

    if (bound != BoundExact && key == e->key && depth < e->depth - 1)
        return;

    e->key      = key;
    e->depth    = depth;
    e->set_bound(bound);
    e->set_gen(gen_);
    e->move     = move;
    e->score    = score_to_tt(score, ply);
    e->eval     = eval;
}

int score_to_tt(int score, int ply)
{
    if (score >= mate_in(PliesMax))
        score += ply;
    else if (score <= mated_in(PliesMax))
        score -= ply;

    assert(abs(score) < ScoreMate);

    return score;
}

int score_from_tt(int score, int ply)
{
    if (score >= mate_in(PliesMax))
        score -= ply;
    else if (score <= mated_in(PliesMax))
        score += ply;

    assert(abs(score) < ScoreMate - ply);

    return score;
}
