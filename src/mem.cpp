#include <fstream>
#include <iterator>
#include <vector>
#include <cassert>
#include <cstdlib>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#ifdef _MSC_VER
#include <emmintrin.h>
#endif

#include "mem.h"

namespace mem {

void * alloc(size_t size)
{
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

void free(void * p)
{
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}

void prefetch(void * p)
{
#ifdef _MSC_VER
    _mm_prefetch((char *)p, _MM_HINT_T0);
#else
    __builtin_prefetch(p);
#endif
}

std::vector<uint8_t> read(std::string filename)
{
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);

    std::streampos bytes = ifs.tellg();

    ifs.seekg(0, std::ios::beg);

    std::vector<uint8_t> vec(bytes);

    if (!ifs.read(reinterpret_cast<char *>(vec.data()), bytes))
        exit(EXIT_FAILURE);

    return vec;
}

void write(const void * p, size_t size, std::string filename)
{
    std::ofstream ofs(filename, std::ios::binary);

    ofs.write(reinterpret_cast<const char *>(p), size);
}

}
