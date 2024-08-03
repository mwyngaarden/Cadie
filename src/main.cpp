#include <iostream>
#include <limits>
#include <thread>
#include <cstdlib>
#include <cstring>
#include "attack.h"
#include "bb.h"
#include "bench.h"
#include "gen.h"
#include "misc.h"
#include "perft.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "see.h"
#include "square.h"
#include "string.h"
#include "tt.h"
#include "uci.h"
#include "uciopt.h"
using namespace std;

static void help(char* exe)
{
    assert(exe != nullptr);

    cout << "Usage: " << exe << " [options]" << endl
         << "Options:" << endl
         << "  bench [depth=N] [file=path] [num=N] [hash=MB] [nodes=N] [random] [time=ms] [mates] [option.K=V]" << endl
         << "  perft [depth=N] [file=path] [num=N] [report=N]" << endl
#ifdef TUNE
         << "  tune <path> [full]" << endl
#endif
         << "  validate" << endl
         << "  help" << endl;
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

    if (argc >= 2) {
        if (strcmp(argv[1], "bench") == 0)
            benchmark(argc - 2, argv + 2);
        else if (strcmp(argv[1], "perft") == 0)
            perft(argc - 2, argv + 2);
#ifdef TUNE
        else if (strcmp(argv[1], "tune") == 0)
            eval_tune(argc - 2, argv + 2);
#endif
        else if (strcmp(argv[1], "help") == 0)
            help(argv[0]);

        return EXIT_SUCCESS;
    }

    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    
    uci_loop();

    return EXIT_SUCCESS;
}
