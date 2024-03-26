#ifndef MEM_H
#define MEM_H

#include <cstdint>

namespace mem {

void * alloc(size_t size);
void free(void * p);
void prefetch(void * p);

}

#endif
