#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "perft.h"
#include "pos.h"
#include "search.h"
#include "string.h"
#include "timer.h"
#include "tt.h"
#include "uci.h"
using namespace std;

struct FenInfo {
    FenInfo(const Tokenizer& fields)
    {
        fen = fields[0];

        for (size_t i = 1; i < fields.size(); i++)
            leaves.push_back(stoull(fields[i]));
    }
    
    string fen;

    vector<i64> leaves;
};

struct PerftInfo {
    size_t num      =  -1;
    size_t depth    =   1;
    size_t report   = 100;

    size_t leaves   =   0;
    size_t illegals =   0;
    size_t micros   =   0;
    size_t cycles   =   0;

    filesystem::path path { "perft.epd" };

    vector<FenInfo> finfos;
};


static i64 perft(Position& pos, size_t depth)
{
    UndoInfo undo = pos.undo_info();

    MoveList moves;

    i64 leaves = 0;

#if 1
    gen_moves(moves, pos, GenMode::Legal);

    if (depth == 1) return moves.size();

    for (auto m : moves) {
        pos.make_move(m);
        leaves += perft(pos, depth - 1);
        pos.unmake_move(undo);
    }
#else
    gen_moves(moves, pos, GenMode::Pseudo);

    for (auto m : moves) {
        if (pos.move_is_legal(m)) {
            if (depth == 1)
                leaves++;
            else {
                pos.make_move(m);
                leaves += perft(pos, depth - 1);
                pos.unmake_move(undo);
            }
        }
    }
#endif

    return leaves;
}

static void perft_input(PerftInfo& pinfo)
{
    ifstream ifs(pinfo.path);

    for (string line; getline(ifs, line); ) {
        if (line.empty() || line[0] == '#') continue;

        Tokenizer fields(line, ',');

        pinfo.finfos.push_back(FenInfo(fields));
    }
}

static void perft_go(PerftInfo &pinfo)
{
    i64 invalid = 0;

    size_t count = min(pinfo.num, pinfo.finfos.size());

    for (size_t i = 0; i < count; i++) {
        const FenInfo& fi = pinfo.finfos[i];

        Position pos(fi.fen);

        const i64 leaves_req = fi.leaves[pinfo.depth - 1];

        Timer timer(true);
        i64 leaves = perft(pos, pinfo.depth);
        timer.stop();

        const i64 micros = timer.elapsed_time<Timer::Micro>();
        const i64 cycles = timer.elapsed_cycles();

        pinfo.leaves += leaves;
        pinfo.micros += micros;
        pinfo.cycles += cycles;

        i64 leaves_diff = leaves - leaves_req;

        invalid += leaves_diff != 0;

        pinfo.illegals += leaves_diff != 0;

        double cklps = 1000 * pinfo.leaves / pinfo.micros;
        
        if ((i + 1) % pinfo.report == 0) {
            stringstream ss;

            ss << setprecision(2) << fixed
               << "n = " << setw(4) << (i + 1) << ' '
               << "d = " << setw(1) << pinfo.depth << ' '
               << "dl = " << setw(1) << leaves_diff << ' '
               << "inv = " << setw(1) << invalid << ' '
               << "cpl = " << setw(4) << pinfo.cycles / pinfo.leaves << ' '
               << "cklps = " << setw(7) << size_t(cklps) << ' '
               << (leaves_diff == 0 ? "PASS" : "FAIL !!!");

            cout << ss.str() << endl;
        }
    }
}

void perft(int argc, char* argv[])
{
    Tokenizer fields(argc, argv);

    PerftInfo pinfo;

    for (size_t i = 0; i < fields.size(); i++) {
        Tokenizer args(fields[i], '=');

        string k = args[0];
        string v = args[1];

        if (k == "depth") {
            size_t n = stoull(v);
            pinfo.depth = n;

        }
        else if (k == "file") {
            filesystem::path p { v };
            pinfo.path = p;
        }
        else if (k == "num") {
            size_t n = stoull(v);
            pinfo.num = n;
        }
        else if (k == "report") {
            size_t n = stoull(v);
            pinfo.report = n;
        }
        else {
            cerr << "Unknown option: " << k << endl;
            return;
        }
    }

    perft_input(pinfo);
    perft_go(pinfo);

    cout << "leaves:   " << pinfo.leaves << endl
         << "millis:   " << pinfo.micros / 1000 << endl
         << "klps:     " << 1000 * pinfo.leaves / pinfo.micros << endl
         << "cpl:      " << pinfo.cycles / pinfo.leaves << endl
         << "illegals: " << pinfo.illegals << endl;
}
