#include <cassert>
#include <cstdlib>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#ifdef _MSC_VER
#include <emmintrin.h>
#endif

#include "mem.h"

void * my_alloc(size_t size)
{
    [[maybe_unused]]
    constexpr size_t alignment = 2 * 1024 * 1024;

    [[maybe_unused]]
    int ret;

    void * p = nullptr;

#ifdef _MSC_VER
    p = _aligned_malloc(size, alignment);
#elif _POSIX_C_SOURCE >= 200112L
    ret = posix_memalign(&p, alignment, size);
    assert(ret == 0);
#else
    p = std::aligned_alloc(alignment, size);
#endif

#if defined(__linux__) && defined(MADV_HUGEPAGE)
    ret = madvise(p, size, MADV_HUGEPAGE);
    assert(ret == 0);
#endif

    return p;
}

void my_free(void * p)
{
#ifdef _MSC_VER
    _aligned_free(p);
#else
    free(p);
#endif
}

void my_prefetch(void * p)
{
#ifdef _MSC_VER
    _mm_prefetch((char *)p, _MM_HINT_T0);
#else
    __builtin_prefetch(p);
#endif
}
