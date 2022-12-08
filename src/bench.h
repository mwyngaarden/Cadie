#ifndef BENCH_H
#define BENCH_H

#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>
#include "pos.h"


struct Sample {
    i64 time;
    i64 nodes;
    size_t depth;
};

struct Bench {
    enum Limit { Time, Depth, Nodes };

    Limit limit     =  Depth;
    size_t num      =    60;
    size_t time     =     0;
    size_t depth    =     8;
    size_t nodes    =     0;
    size_t hash     =    32;
    bool exc        =  true;
    bool rand       = false;

    std::filesystem::path path { "bench.epd" };

    std::vector<Position> positions;

    std::vector<Sample> samples;
};

void benchmark(int argc, char* argv[]);

#endif
