#ifndef MEM_H
#define MEM_H

#include <cstdint>

void * my_alloc(size_t size);
void my_free(void * p);
void my_prefetch(void * p);

#endif
