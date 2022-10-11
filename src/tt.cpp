// includes

#include <bit>

#ifdef _MSC_VER
#include <emmintrin.h>
#endif

#include <cassert>
#include <cstdint>
#include "tt.h"

// namespaces

using namespace std;

// constants

constexpr size_t TTSizeMB = 16;

// variables

TransTable thtable(TTSizeMB);

// prototypes

// functions

TransTable::TransTable(size_t size_mb)
{
    size_mb_ = size_mb;

    count_ = size_mb * 1024 * 1024 / sizeof(TransTable::Cluster);

    while (popcount(count_) > 1) count_ &= count_ - 1;

    assert(popcount(count_) == 1);

    mask_ = count_ - 1;

    clusters_.resize(count_);

    reset();
}

bool TransTable::get(u64 key, int ply, Move& move, i16& eval, i16& score, i8& depth, u8& bound)
{
    ++reads;

    Cluster& c = clusters_[key & mask_];

    key = mod_key(key);

    for (std::size_t i = 0; i < ClusterCount; i++) {
        Entry& e = c.entries[i];

        if (e.key == key) {
            e.gen = gen_;

            depth   = e.depth;
            bound   = e.bound;
            move    = e.move;
            score   = score_from_tt(e.score, ply);
            eval    = e.eval;

            ++hits;

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
        i8 a =           e->depth - (gen_ -           e->gen);
        i8 b = c.entries[i].depth - (gen_ - c.entries[i].gen);

        if (a >= b)
            e = &c.entries[i];
    }

    if (i != ClusterCount)
        e = &c.entries[i];

    if (bound != BoundExact && key == e->key && depth < e->depth - 1)
        return;

    e->key      = key;
    e->depth    = depth;
    e->bound    = bound;
    e->gen      = gen_;
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
