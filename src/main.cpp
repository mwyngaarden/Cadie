#include <iostream>
#include <limits>
#include <thread>
#include <cstdlib>
#include <cstring>
#include "attacks.h"
#include "bb.h"
#include "bench.h"
#include "gen.h"
#include "misc.h"
#include "perft.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "square.h"
#include "string.h"
#include "tt.h"
#include "uci.h"
#include "uciopt.h"
using namespace std;

static void help(string exe)
{
    cout << "Usage: " << exe << " [options]" << endl
         << "Options:" << endl
         << "  bench [depth=N] [file=path] [num=N] [hash=MB] [nodes=N] [random] [time=ms] [mates] [option.K=V]" << endl
         << "  perft [depth=N] [file=path] [num=N] [report=N]" << endl;
}

int main(int argc, char* argv[])
{
    gstats.time_init = Timer::now();

    // Maintain order!
    search_init();
    zob::init();
    uci_init();
    eval_init();
    bb::init();

    Tokenizer tokens(argc, argv);

    if (tokens.size() >= 2) {
        if (tokens[1] == "bench")
            benchmark(argc - 2, argv + 2);
        else if (tokens[1] == "perft")
            perft(argc - 2, argv + 2);
        else
            help(tokens[0]);

        return EXIT_SUCCESS;
    }

    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    
    uci_loop();

    return EXIT_SUCCESS;
}
