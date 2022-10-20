#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "eval.h"
#include "gen.h"
#include "move.h"
#include "perft.h"
#include "pos.h"
#include "search.h"
#include "string.h"
#include "tt.h"
#include "types.h"
#include "uci.h"

using namespace std;
struct FenInfo {
    string fen;
    array<i64, 9> leafs;

    FenInfo(const string& f, i64 n0, i64 n1, i64 n2, i64 n3, i64 n4, i64 n5, i64 n6 = 0, i64 n7 = 0, i64 n8 = 0)
        : fen(f)
    {
        leafs[0] = n0;
        leafs[1] = n1;
        leafs[2] = n2;
        leafs[3] = n3;
        leafs[4] = n4;
        leafs[5] = n5;
        leafs[6] = n6;
        leafs[7] = n7;
        leafs[8] = n8;
    }
};

static vector<FenInfo> perft_read_epd(const string& filename);

static i64 perft(Position& pos, int depth, int height, i64& illegal_moves);

i64 perft(int depth, i64& illegal_moves, i64& total_microseconds, i64& total_cycles, bool startpos)
{
    i64 total_leafs = 0;
    vector<FenInfo> fen_info = perft_read_epd("perft.epd");
    i64 invalid = 0;

    for (size_t i = 0; i < fen_info.size(); i++) {
        const FenInfo& fi = fen_info[i];

        Position pos(fi.fen);

        int sscore = pos.in_check() ? 0 : eval(pos, -ScoreMate, ScoreMate);

        //if (Debug) {
        if (false) {
            if (fi.leafs[depth - 1] == 0)
                continue;

            search_reset();

            Position pos_flipped = pos;

            pos_flipped.swap_sides();

            int score_flipped = eval(pos_flipped, -ScoreMate, ScoreMate);

            cout << pos.to_fen() << " : " << sscore << " / " << score_flipped << " : " << (i + 1) << endl;

            assert(sscore == -score_flipped);

            //for (int d = 1; d < 7; d++) {
            for (int d = 1; d < 17; d++) {
                //snodes = 0;
                Move pv[PliesMax];

                search_iter(pos, d, pv);

                ostringstream oss;

                for (int j = 0; pv[j] != MoveNone; j++) {
                    if (j > 0)
                        oss << ' ';
                    oss << pv[j].to_string();
                }
            }

            search_info.uci_bestmove();
        }

        const i64 want_leafs = fi.leafs[depth - 1];
        auto t0 = chrono::high_resolution_clock::now();
        i64 rdtsc0 = __rdtsc();
        i64 leafs = perft(pos, depth, 0, illegal_moves);
        auto t1 = chrono::high_resolution_clock::now();
        i64 rdtsc1 = __rdtsc();
        const auto microseconds = chrono::duration_cast<chrono::microseconds>(t1 - t0);
        const i64 cycles = rdtsc1 - rdtsc0;

        total_microseconds += microseconds.count();
        total_cycles += cycles;
        i64 diff_leafs = leafs - want_leafs;
        invalid += diff_leafs != 0;
        total_leafs += leafs;
        double cum_mlps = double(total_leafs) / (double(total_microseconds) / 1e+6) / 1e+6;

        if ((i + 1) % 100 == 0) {
            stringstream ss;
            ss << setprecision(2) << fixed
               << "n = " << setw(4) << (i + 1) << ' '
               << "d = " << setw(1) << depth << ' '
               << "dl = " << setw(1) << diff_leafs << ' '
               << "inv = " << setw(1) << invalid << ' '
               << "cmlps = " << setw(5) << cum_mlps << ' '
               << (diff_leafs == 0 ? "PASS" : "FAIL");

            cout << ss.str() << endl;
        }

        if (startpos)
            break;
    }
    return total_leafs;
}

i64 perft(Position& pos, int depth, int height, i64& illegal_moves)
{
    if (depth == 0)
        return 1;

    MoveList moves;
    i64 legal_moves = 0;

    gen_moves(moves, pos, Legal);

    for (const auto& m : moves) {
        if (!pos.move_is_legal(m)) {
            illegal_moves++;
            continue;
        }

        Position tp(pos);
        
        MoveUndo undo;

        bool gives_check = pos.move_is_check(m);

#if 1
        pos.make_move(m, undo);

        assert(!pos.side_attacks(pos.side(), pos.king_sq(flip_side(pos.side()))));

        assert(gives_check == pos.in_check());

        if (gives_check != pos.in_check()) {
            cout << tp.dump() << endl;
            cout << pos.dump() << endl;

            gives_check = tp.move_is_check(m);

            exit(1);

        }

        legal_moves += perft(pos, depth - 1, height + 1, illegal_moves);

        pos.unmake_move(m, undo);
#else
        Position backup = pos;

        pos.make_move(m, undo);

        assert(!pos.side_attacks(pos.side(), pos.king_sq(pos.side())));

        legal_moves += perft(pos, depth - 1, height + 1, illegal_moves);

        pos = backup;
#endif
    }

    return legal_moves;
}

vector<FenInfo> perft_read_epd(const string& filename)
{
    vector<FenInfo> fen_info;
    ifstream ifs(filename);
    string line;

    while (getline(ifs, line)) {
        assert(!line.empty());

        if (line[0] == '#') continue;

        Tokenizer list(line, ',');

        assert(list.size() >= 7);

        const string& fen = list[0];

        i64 n0 = stoll(list[1]);
        i64 n1 = stoll(list[2]);
        i64 n2 = stoll(list[3]);
        i64 n3 = stoll(list[4]);
        i64 n4 = stoll(list[5]);
        i64 n5 = stoll(list[6]);
        i64 n6 = list.size() >=  8 ? stoll(list[7]) : 0;
        i64 n7 = list.size() >=  9 ? stoll(list[8]) : 0;
        i64 n8 = list.size() >= 10 ? stoll(list[9]) : 0;

        fen_info.emplace_back(fen, n0, n1, n2, n3, n4, n5, n6, n7, n8);
    }

    return fen_info;
}

void perft_validate(int argc, char* argv[])
{
    int depth = stoi(argv[0]);
    
    bool startpos = argc >= 2 && strcmp(argv[1], "-s") == 0;

    i64 illegal_moves = 0;
    i64 total_microseconds = 0;
    i64 total_cycles = 0;
    i64 total_leafs = perft(depth, illegal_moves, total_microseconds, total_cycles, startpos);

    cout << "leafs: " << total_leafs << endl;
    cout << "milliseconds: " << double(total_microseconds) / 1000.0 << endl;
    cout << "klps: " << 1000.0 * double(total_leafs) / double(total_microseconds) << endl;
    cout << "cpl: " << double(total_cycles) / double(total_leafs) << endl;
    cout << "illegal moves: " << illegal_moves << " (" << 100.0 * illegal_moves / total_leafs << " %)" << endl;
}
