#include <iostream>
#include <limits>
#include <thread>
#include <cstdlib>
#include <cstring>
#include "bench.h"
#include "gen.h"
#include "perft.h"
#include "piece.h"
#include "pos.h"
#include "search.h"
#include "see.h"
#include "square.h"
#include "string.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "uciopt.h"
using namespace std;

int main(int argc, char* argv[])
{
    // Maintain order!
    square_init();
    search_init();
    piece_init();
    eval_init();
    gen_init();
    zobrist_init();
    uci_init();

    if (argc >= 2) {
        if (strcmp(argv[1], "bench") == 0)
            benchmark(argc - 2, argv + 2);
        else if (strcmp(argv[1], "perft") == 0)
            perft(argc - 2, argv + 2);
        else if (strcmp(argv[1], "tune") == 0)
            eval_tune(argc - 2, argv + 2);
        else if (strcmp(argv[1], "validate") == 0) {
            zobrist_validate();
            see_validate();
        }

        return EXIT_SUCCESS;
    }

    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    
    uci_loop();

    return EXIT_SUCCESS;
}
