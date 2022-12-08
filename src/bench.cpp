#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "bench.h"
#include "string.h"
#include "types.h"
#include "tt.h"
using namespace std;

static void benchmark_input (Bench& bench);
static void benchmark_go    (Bench& bench);

void benchmark(int argc, char* argv[])
{
    Tokenizer fields(argc, argv);

    Bench bench;

    for (size_t i = 0; i < fields.size(); i++) {
        Tokenizer args(fields[i], '=');

        string k = args.get(0);
        string v = args.get(1, "");

        if (k == "depth") {
            assert(args.size() == 2);
            bench.limit = Bench::Depth;
            size_t n = stoull(v);
            assert(n > 0 && n <= DepthMax);
            bench.depth = n;
        }
        else if (k == "filename") {
            assert(args.size() == 2);
            filesystem::path p { v };
            assert(filesystem::exists(p));
            bench.path = p;
        }
        else if (k == "hash") {
            assert(args.size() == 2);
            size_t n = stoull(v);
            assert(n >= 1 && n <= 1024);
            bench.hash = n;
        }
        else if (k == "nodes") {
            assert(args.size() == 2);
            bench.limit = Bench::Nodes;
            size_t n = stoull(v);
            assert(n > 0);
            bench.nodes = n;
        }
        else if (k == "num") {
            assert(args.size() == 2);
            size_t n = stoull(v);
            assert(n >= 1);
            bench.num = n;
        }
        else if (k == "rand") {
            assert(args.size() == 1);
            bench.rand = true;
        }
        else if (k == "time") {
            assert(args.size() == 2);
            bench.limit = Bench::Time;
            size_t n = stoull(v);
            assert(n < 3600 * 1000);
            bench.time = n;
        }
        else if (k == "inc")
            bench.exc = false;
        else
            assert(false);
    }

    benchmark_input(bench);
    benchmark_go(bench);
}

template <class T = string>
string ralign(T v, size_t n)
{
    ostringstream oss;
    oss << setfill(' ') << setw(n + 2) << right << v;
    return oss.str();
}

void benchmark_go(Bench &bench)
{
    if (bench.rand) {
        random_device rd;
        mt19937 g(rd());

        shuffle(bench.positions.begin(), bench.positions.end(), g);
    }

    thtable = TransTable(bench.hash);
    thtable.init();

    Timer& timer = gstats.stimer;

    gstats.benchmarking = true;
    gstats.exc          = bench.exc;

    for (const auto& pos : bench.positions) {
        sinfo = SearchInfo(pos);

        move_stack.clear();
        move_stack.add(MoveNone);
        move_stack.add(MoveNone);

        key_stack.clear();
        key_stack.add(pos.key());
            
        slimits = SearchLimits(true);

        if (bench.limit == Bench::Depth)
            slimits.depth = bench.depth;
        else if (bench.limit == Bench::Nodes)
            slimits.nodes = bench.nodes;
        else if (bench.limit == Bench::Time)
            slimits.movetime = bench.time;

        search_start();
        
        if (gstats.num == bench.num) break;
    }

    i64 ttime_ns    = timer.accrued_time<Timer::Nano>().count();
    double ttime_s  = ttime_ns / 1E+9;

    i64 tcycles     = timer.accrued_cycles();
    i64 ttime       = timer.accrued_time().count();
    i64 hz          = tcycles / ttime_s;
    i64 tmoves      = gstats.moves_count;
    i64 snodes      = gstats.nodes_search;
    i64 qnodes      = gstats.nodes_qsearch;
    i64 tnodes      = snodes + qnodes;
    i64 num         = gstats.num;
    i64 iotime      = gstats.iotimer.accrued_time().count();
    i64 etime       = gstats.time_eval_ns;
    i64 gtime       = gstats.time_gen_ns;

    vector<string> units_time { "ns", "us", "ms", "s " };
    vector<string> units_speed { "nps ", "knps", "mnps" };

    cerr << "moves      = " << ralign<i64>(tmoves, 11) << endl
         << "nodes      = " << ralign<i64>(tnodes, 11) << endl
         << "time       = " << ralign(Timer::to_string(ttime_ns, units_time), 14) << endl
         << "cycles     = " << ralign<i64>(tcycles, 11) << endl
         << "cpu        = " << ralign(Timer::to_string(hz, {"Hz ", "kHz", "MHz", "GHz"}), 15) << endl
         << endl
         << "Positions" << endl
         << " depth min  = " << ralign<i64>(gstats.depth_min, 10) << endl
         << " depth mean = " << ralign<i64>((double)gstats.depth_sum / num, 10) << endl
         << " depth max  = " << ralign<i64>(gstats.depth_max, 10) << endl
         << endl
         << " nodes min  = " << ralign<i64>(gstats.nodes_min, 10) << endl
         << " nodes mean = " << ralign<i64>(tnodes / num, 10) << endl
         << " nodes max  = " << ralign<i64>(gstats.nodes_max, 10) << endl
         << endl
         << " time min   = " << ralign(Timer::to_string(gstats.time_min, units_time), 13) << endl
         << " time mean  = " << ralign(Timer::to_string(ttime / num, units_time), 13) << endl
         << " time max   = " << ralign(Timer::to_string(gstats.time_max, units_time), 13) << endl
         << endl
         << "Overall" << endl
         << " nodes min  = " << ralign(Timer::to_string(gstats.nps_min, units_speed), 15) << endl
         << " nodes mean = " << ralign(Timer::to_string(tnodes / ttime_s, units_speed), 15) << endl
         << " nodes max  = " << ralign(Timer::to_string(gstats.nps_max, units_speed), 15) << endl
         << endl
         << "unfinished = " << ralign<i64>(gstats.unfinished, 11) << endl
         << "stalemate  = " << ralign<i64>(gstats.stalemate, 11) << endl
         << "checkmate  = " << ralign<i64>(gstats.checkmate, 11) << endl
         << "total      = " << ralign<i64>(gstats.num, 11) << endl
         << endl
         << "time io    = " << ralign(Timer::to_string(iotime, units_time), 14) << endl
         << "time eval  = " << ralign(Timer::to_string(etime, units_time), 14) << endl
         << "time gen   = " << ralign(Timer::to_string(gtime, units_time), 14) << endl;
}

void benchmark_input(Bench& bench)
{
    ifstream ifs(bench.path);
    assert(ifs.is_open());

    for (string line; getline(ifs, line); ) {
        if (line.empty() || line[0] == '#') continue;

        Tokenizer fields(line, ',');

        assert(fields.size() > 0);

        bench.positions.emplace_back(fields[0]);
    }
}
