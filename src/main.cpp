#include <iostream>
#include <limits>
#include <thread>
#include <cstdlib>
#include <cstring>
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
    square_init();
    search_init();
    piece_init();
    eval_init();
    gen_init();
    zobrist_init();
    uci_init();

    if (argc >= 2) {
        if (strcmp(argv[1], "-p") == 0)
            perft_validate(argc - 2, argv + 2);
        else if (strcmp(argv[1], "-e") == 0)
            eval_error(argc - 2, argv + 2);
        else if (strcmp(argv[1], "-v") == 0) {
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
