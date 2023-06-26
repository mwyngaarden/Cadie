#include <bit>
#include <cassert>
#include <cstdint>
#include "search.h"
#include "tt.h"

using namespace std;

int score_to_tt(int score, int ply)
{
    if (score >= mate_in(PliesMax))
        score += ply;
    else if (score <= mated_in(PliesMax))
        score -= ply;

    assert(abs(score) <= ScoreMate);

    return score;
}

int score_from_tt(int score, int ply)
{
    if (score >= mate_in(PliesMax))
        score -= ply;
    else if (score <= mated_in(PliesMax))
        score += ply;

    assert(abs(score) <= ScoreMate - ply);

    return score;
}

TT ttable(TTSizeMBDefault);

TT::TT(size_t mb)
{
    assert(mb >= TTSizeMBMin && mb <= TTSizeMBMax);

    size_  = bit_floor(mb * 1024 * 1024);
    count_ = size_ / sizeof(Entry);
    mask_  = count_ - 1;
}

void TT::init()
{
    assert(entries_.empty());

    entries_.resize(count_);
}

bool TT::get(Entry& dst, u64 key, int ply)
{
    gets_++;
    
    dst.lock = Entry::calc_lock(key);
    dst.gen = gen_;
    
    Entry& src = entries_[key & mask_];

    if (dst.lock == src.lock) {
        hits_++;

        dst.move  = src.move;
        dst.score = score_from_tt(src.score, ply);
        dst.eval  = src.eval;
        dst.depth = src.depth;
        dst.bound = src.bound;

        assert(dst.is_valid());

        return true;
    }

    return false;
}

void TT::set(Entry& src, u64 key)
{
    assert(src.is_valid());
    
    Entry * dst = &entries_[key & mask_];

    src.lock = Entry::calc_lock(key);
    src.gen = gen_;

    bool write = dst->lock == 0
        || (src.bound == BoundExact)
        || (dst->gen != src.gen)
        || (dst->bound != BoundExact && src.depth >= dst->depth)
        || (dst->lock == src.lock && src.depth + 1 >= dst->depth);
    
    if (write) *dst = src;
}

