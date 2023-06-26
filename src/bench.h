#ifndef BENCH_H
#define BENCH_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#include "pos.h"


struct Bench {
    std::size_t num     =   60;
    std::size_t time    =    0;
    std::size_t depth   =   10;
    std::size_t nodes   =    0;
    std::size_t hash    =    1;

    bool exc        =  true;
    bool rand       = false;
    bool barenodes  = false;
    bool baretime   = false;

    std::map<std::string, std::string> opts;

    std::filesystem::path path { "perft.epd" };

    std::vector<Position> positions;
};

void benchmark(int argc, char* argv[]);

#endif
