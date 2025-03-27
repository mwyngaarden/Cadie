#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "bench.h"
#include "misc.h"
#include "search.h"
#include "string.h"
#include "timer.h"
#include "tt.h"
#include "uci.h"
using namespace std;

struct Bench {
    std::size_t num     = 30;
    std::size_t time    =  0;
    std::size_t depth   = 13;
    std::size_t nodes   =  0;
    std::size_t hash    =  8;

    bool exc_mated  = true;
    bool rand       = false;
    bool barenodes  = false;
    bool baretime   = false;

    std::map<std::string, std::string> opts;

    std::filesystem::path path { "bench.epd" };

    std::vector<Position> positions;
};

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

        if (k == "barenodes")
            bench.barenodes = true;
        else if (k == "baretime")
            bench.baretime = true;
        else if (k == "depth") {
            size_t n = stoull(v);
            bench.depth = n;
        }
        else if (k == "file") {
            filesystem::path p { v };
            bench.path = p;
        }
        else if (k == "hash") {
            size_t n = stoull(v);
            bench.hash = n;
        }
        else if (k == "mates")
            bench.exc_mated = false;
        else if (k == "nodes") {
            size_t n = stoull(v);
            bench.nodes = n;
        }
        else if (k == "num") {
            size_t n = stoull(v);
            bench.num = n;
        }
        else if (k.starts_with("option."))
            bench.opts[k.substr(7)] = v;
        else if (k == "random")
            bench.rand = true;
        else if (k == "time") {
            size_t n = stoull(v);
            bench.time = n;
        }
        else {
            cerr << "Unknown option: " << k << endl;
            return;
        }
    }

    benchmark_input(bench);
    benchmark_go(bench);
}

void benchmark_go(Bench &bench)
{
    if (bench.rand) {
        random_device rd;
        mt19937 gen(rd());

        shuffle(bench.positions.begin(), bench.positions.end(), gen);
    }

    ttable = TT(bench.hash);
    ttable.init();

    for (auto kv : bench.opts) {
        string s = "setoption name " + kv.first + " value " + kv.second;
        uci_setoption(s);
    }

    Timer& timer = gstats.stimer;

    gstats.exc_mated = bench.exc_mated;

    bool quiet = bench.barenodes || bench.baretime;

    for (const auto& pos : bench.positions) {

        if (!quiet)
            cerr << "Position " << setw(4) << right << (gstats.num + 1) << " - " << pos.to_fen() << endl;

        search_reset();

        mstack.clear();
        mstack.add(Move::None());
        mstack.add(Move::None());

        kstack.clear();
        kstack.add(pos.key());
            
        sl = SearchLimits();

        sl.depth = bench.depth;
        sl.nodes = bench.nodes;
        sl.move_time = bench.time;

        si = SearchInfo(pos);

        si.reset();
        si.timer.start();

        search_start();
        
        if (gstats.num == bench.num) break;
    }

    i64 ttime_ns    = timer.accrued_time<Timer::Nano>().count();
    double ttime_s  = ttime_ns / 1e+9;
    
    i64 ttime           = timer.accrued_time().count();
    i64 tnodes          = gstats.nodes_sum;
    i64 num             = gstats.num;
    
    vector<string> units_time { "ns", "us", "ms", "s " };
    vector<string> units_speed { "nps ", "knps", "mnps" };
    
    if (bench.barenodes) {
        cerr << tnodes << endl;
        return;
    }
    if (bench.baretime) {
        cerr << (ttime_ns / 1000 / 1000) << endl;
        return;
    }

    cerr << endl
         << "Positions"       << endl
         << format(" depth min   = {:13}", gstats.depth_min) << endl
         << format(" depth mean  = {:13}", gstats.depth_sum / num) << endl
         << format(" depth max   = {:13}", gstats.depth_max) << endl
         << format(" depth sum   = {:13}", gstats.depth_sum) << endl
         << endl

         << format(" nodes min   = {:13}", gstats.nodes_min) << endl
         << format(" nodes mean  = {:13}", tnodes / num) << endl
         << format(" nodes max   = {:13}", gstats.nodes_max) << endl
         << endl

         << format(" time min    = {:>16}", Timer::to_string(gstats.time_min, units_time)) << endl
         << format(" time mean   = {:>16}", Timer::to_string(ttime / num, units_time)) << endl
         << format(" time max    = {:>16}", Timer::to_string(gstats.time_max, units_time)) << endl
         << endl

         << "Overall"         << endl
         << format(" nodes min   = {:>18}", Timer::to_string(gstats.nps_min, units_speed)) << endl
         << format(" nodes mean  = {:>18}", Timer::to_string(tnodes / ttime_s, units_speed)) << endl
         << format(" nodes max   = {:>18}", Timer::to_string(gstats.nps_max, units_speed)) << endl
         << endl

         << format("normal       = {:13}", gstats.normal) << endl
         << format("stalemate    = {:13}", gstats.stalemate) << endl
         << format("checkmate    = {:13}", gstats.checkmate) << endl
         << format("total        = {:13}", gstats.num) << endl
         << endl

         << format("et hitrate   = {:13}", ttable.hitrate()) << endl
         << format("nodes        = {:13}", tnodes) << endl
         << format("nps          = {:13}", int(tnodes / ttime_s)) << endl
         << format("time         = {:>16}", Timer::to_string(ttime_ns, { "ns", "us", "ms" })) << endl;
}

void benchmark_input(Bench& bench)
{
    ifstream ifs(bench.path);

    for (string line; getline(ifs, line); ) {
        if (line.empty() || line[0] == '#') continue;

        Tokenizer fields(line, ',');

        bench.positions.emplace_back(fields[0]);
    }
}
