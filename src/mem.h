#ifndef MEM_H
#define MEM_H

#include <string>
#include <vector>
#include <cstdint>

namespace mem {

void * alloc(size_t size);
void free(void * p);
void prefetch(void * p);

std::vector<uint8_t> read(std::string filename);
void write(const void * p, size_t size, std::string filename);

}

#endif
