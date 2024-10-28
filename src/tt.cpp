#include <bit>
#include <cassert>
#include <cstdint>
#include "search.h"
#include "tt.h"

using namespace std;

TT ttable(TTSizeMBDefault);

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

    const Entry& src = entries_[key & mask_];

    Entry::Lock lock = Entry::make_lock(key);

    // The non-zero bound check prevents collisions with uninitialized entries
    if (src.lock == lock && src.bound) {
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

void TT::set(u64 key, Move m, i16 score, i16 eval, i8 depth, u8 bound, int ply)
{
    Entry * dst = &entries_[key & mask_];

    Entry::Lock lock = Entry::make_lock(key);

    bool write = (dst->lock == 0)
              || (bound == BoundExact)
              || (dst->gen != gen_)
              || (dst->bound != BoundExact && depth >= dst->depth)
              || (dst->lock == lock && depth + 3 > dst->depth);

    if (write) {
        if (m || dst->lock != lock) dst->move = m;

        dst->lock   = lock;
        dst->score  = score_to_tt(score, ply);
        dst->eval   = eval;
        dst->depth  = depth;
        dst->gen    = gen_;
        dst->bound  = bound;
    }
}
