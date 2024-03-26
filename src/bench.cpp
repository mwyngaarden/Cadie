#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <random>
#include <sstream>
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

        if (k == "barenodes") {
            assert(args.size() == 1);
            bench.barenodes = true;
        }
        else if (k == "baretime") {
            assert(args.size() == 1);
            bench.baretime = true;
        }
        else if (k == "depth") {
            assert(args.size() == 2);
            size_t n = stoull(v);
            assert(n > 0 && n <= DepthMax);
            bench.depth = n;
        }
        else if (k == "file") {
            assert(args.size() == 2);
            filesystem::path p { v };
            assert(filesystem::exists(p));
            bench.path = p;
        }
        else if (k == "hash") {
            assert(args.size() == 2);
            size_t n = stoull(v);
            assert(n >= 1 && n <= 128 * 1024);
            bench.hash = n;
        }
        else if (k == "mates") {
            assert(args.size() == 1);
            bench.exc = false;
        }
        else if (k == "nodes") {
            assert(args.size() == 2);
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
        else if (k.starts_with("option.")) {
            assert(args.size() == 2);
            bench.opts[k.substr(7)] = v;
        }
        else if (k == "random") {
            assert(args.size() == 1);
            bench.rand = true;
        }
        else if (k == "time") {
            assert(args.size() == 2);
            size_t n = stoull(v);
            assert(n < 3600 * 1000);
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

template <typename T>
string ralign(T v, size_t n)
{
    ostringstream oss;
    oss.imbue(locale(""));
    oss << setfill(' ') << setw(n + 2) << setprecision(2) << fixed << right << v;
    return oss.str();
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

    gstats.exc = bench.exc;

    bool quiet = bench.barenodes || bench.baretime;

    for (const auto& pos : bench.positions) {

        if (!quiet)
            cerr << "Position " << setw(4) << right << (gstats.num + 1) << " - " << pos.to_fen() << endl;

        search_reset();

        sinfo = SearchInfo(pos);

        mstack.clear();
        mstack.add(MoveNone);
        mstack.add(MoveNone);

        kstack.clear();
        kstack.add(pos.key());
            
        slimits = SearchLimits();

        slimits.depth = bench.depth;
        slimits.nodes = bench.nodes;
        slimits.movetime = bench.time;

        sinfo.timer.start();

        search_start();
        
        if (gstats.num == bench.num) break;
    }

    i64 ttime_ns    = timer.accrued_time<Timer::Nano>().count();
    double ttime_s  = ttime_ns / 1e+9;
    
    i64 tevals          = gstats.evals_count;
    double etime        = gstats.time_eval_ns;
    i64 eps             = etime == 0 ? 0 : 1e+9 * tevals / etime;

    i64 tcycles         = timer.accrued_cycles();
    i64 ttime           = timer.accrued_time().count();
    i64 hz              = tcycles / ttime_s;
    i64 tnodes          = gstats.nodes_sum;
    i64 num             = gstats.num;
    i64 gtime           = gstats.time_gen_ns;

    i64 bmups           = gstats.bm_updates;
    i64 bmstable        = gstats.bm_stable;
    
    i64 gcycpseudo      = gstats.cycles_gen[size_t(GenMode::Pseudo)];
    i64 gcyclegal       = gstats.cycles_gen[size_t(GenMode::Legal)];
    i64 gcyctactical    = gstats.cycles_gen[size_t(GenMode::Tactical)];
    i64 gcycles         = gcycpseudo + gcyclegal + gcyctactical;
    
    i64 gcpseudo        = gstats.calls_gen[size_t(GenMode::Pseudo)];
    i64 gclegal         = gstats.calls_gen[size_t(GenMode::Legal)];
    i64 gctactical      = gstats.calls_gen[size_t(GenMode::Tactical)];
    i64 gcalls          = gcpseudo + gclegal + gctactical;
    
    vector<string> units_time { "ns", "us", "ms", "s " };
    vector<string> units_speed { "nps ", "knps", "mnps" };
    vector<string> units_hertz { "Hz ", "kHz", "MHz", "GHz" };
    
    if (bench.barenodes) {
        cerr << tnodes << endl;
        return;
    }
    if (bench.baretime) {
        cerr << (ttime_ns / 1000 / 1000) << endl;
        return;
    }

    if (!gcycles && !gcalls) gcpseudo = gclegal = gctactical = gcalls = gcycpseudo = gcyclegal = gcyctactical = gcycles = 1;

    cerr << "Positions"       << endl
         << " depth min   = " << ralign<i64>(gstats.depth_min,             11) << endl
         << " depth mean  = " << ralign<i64>(1.0 * gstats.depth_sum / num, 11) << endl
         << " depth max   = " << ralign<i64>(gstats.depth_max,             11) << endl
         << " depth sum   = " << ralign<i64>(gstats.depth_sum,             11) << endl 
         << endl
         << " sdepth min  = " << ralign<i64>(gstats.seldepth_min,             11) << endl
         << " sdepth mean = " << ralign<i64>(1.0 * gstats.seldepth_sum / num, 11) << endl
         << " sdepth max  = " << ralign<i64>(gstats.seldepth_max,             11) << endl
         << " sdepth sum  = " << ralign<i64>(gstats.seldepth_sum,             11) << endl 
         << endl
         << " moves max   = " << ralign<i64>(gstats.moves_max, 11) << endl
         << endl
         << " bm updates  = " << ralign<i64>(bmups,                   11) << endl
         << " bm u/n      = " << ralign<double>(1.0 * bmups / num,    11) << endl
         << " bm stable   = " << ralign<i64>(bmstable,                11) << endl
         << " bm s/n      = " << ralign<double>(1.0 * bmstable / num, 11) << endl
         << endl
         << " nodes min   = " << ralign<i64>(gstats.nodes_min, 11) << endl
         << " nodes mean  = " << ralign<i64>(tnodes / num,     11) << endl
         << " nodes max   = " << ralign<i64>(gstats.nodes_max, 11) << endl
         << endl
         << " time min    = " << ralign(Timer::to_string(gstats.time_min, units_time), 14) << endl
         << " time mean   = " << ralign(Timer::to_string(ttime / num, units_time),     14) << endl
         << " time max    = " << ralign(Timer::to_string(gstats.time_max, units_time), 14) << endl
         << endl
         << "Move Gen"        << endl
         << " pseudos     = " << ralign(gcycpseudo,   11) << " cycles" << ralign(gcpseudo,   12) << " calls" << ralign(100 * gcycpseudo / gcycles,   4) << " %" << ralign(gcycpseudo / gcpseudo,     9) << " cycles/call" << endl
         << " legal       = " << ralign(gcyclegal,    11) << " cycles" << ralign(gclegal,    12) << " calls" << ralign(100 * gcyclegal / gcycles,    4) << " %" << ralign(gcyclegal / gclegal,       9) << " cycles/call" << endl
         << " tactical    = " << ralign(gcyctactical, 11) << " cycles" << ralign(gctactical, 12) << " calls" << ralign(100 * gcyctactical / gcycles, 4) << " %" << ralign(gcyctactical / gctactical, 9) << " cycles/call" << endl
         << " total       = " << ralign(gcycles,      11) << " cycles" << ralign(gcalls,     12) << " calls" << ralign(100, 4)                          << " %" << ralign(gcycles / gcalls,          9) << " cycles/call" << endl
         << endl
         << "Overall"         << endl
         << " nodes min   = " << ralign(Timer::to_string(gstats.nps_min, units_speed),   16) << endl
         << " nodes mean  = " << ralign(Timer::to_string(tnodes / ttime_s, units_speed), 16) << endl
         << " nodes max   = " << ralign(Timer::to_string(gstats.nps_max, units_speed),   16) << endl
         << endl
         << "normal       = " << ralign<i64>(gstats.normal,     11) << endl
         << "stalemate    = " << ralign<i64>(gstats.stalemate,  11) << endl
         << "checkmate    = " << ralign<i64>(gstats.checkmate,  11) << endl
         << "total        = " << ralign<i64>(gstats.num,        11) << endl
         << endl
         << "time eval    = " << ralign(Timer::to_string(i64(etime), units_time), 14) << endl
         << "time gen     = " << ralign(Timer::to_string(gtime, units_time), 14) << endl
         << endl
         << "tests check  = " << ralign<i64>(gstats.ctests, 11) << endl
         << "tests see    = " << ralign<i64>(gstats.stests, 11) << endl
         << endl
         << "evals        = " << ralign<i64>(tevals, 11) << endl
         << "eps          = " << ralign<i64>(eps, 11) << endl
         << "nodes        = " << ralign<i64>(tnodes, 11) << endl
         << "nps          = " << ralign<i64>(tnodes / ttime_s, 11) << endl
         << "time         = " << ralign(Timer::to_string(ttime_ns, { "ns", "us", "ms" }), 14) << endl
         << "cpu          = " << ralign(Timer::to_string(hz, units_hertz),      15) << endl;
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
