#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include "search.h"
#include "string.h"
#include "timer.h"
#include "tt.h"
#include "uci.h"
#include "uciopt.h"
using namespace std;

fstream logfile;

GlobalStats gstats;

std::thread sthread;

enum class Direction { In, Out };

static void uci_go          (const string& s);
static void uci_position    (const string& s);
static void uci_ucinewgame  ();
static void uci_stop        ();
static void uci_uci         ();
static void uci_dump        ();

static string uci_log_filename();

template <Direction D>
static void uci_log         (const string& s);

// UCI options with default values

bool NMPruning          =  true;
int  NMPruningDepthMin  =     2;

bool SingularExt        =  true;
int  SEDepthMin         =     6;
int  SEDepthOffset      =     3;

bool StaticNMP          =  true;
int  StaticNMPDepthMax  =     7;
int  StaticNMPFactor    =   100;

bool Razoring           =  true;
int  RazoringFactor     =   200;

int  AspMargin          =    10;

int  UciOverhead        =    15;
bool UciLog             = false;

int  FutilityFactor     =    60;

bool EasyMove           =  true;
int  EMMargin           =   400;
int  EMShare            =    25;

UCIOptionList opt_list;

void uci_init()
{
    cout << "Cadie " << CADIE_VERSION << " (" << CADIE_DATE << ' ' << CADIE_TIME << ") by Martin Wyngaarden" << endl;
    cout << "Debug = " << Debug << " PROFILE = " << PROFILE << endl;

    opt_list.add(UCIOption("Hash", TTSizeMBMin, ttable.size_mb(), TTSizeMBMax));
    opt_list.add(UCIOption("Clear Hash"));

    opt_list.add(UCIOption("Razoring", Razoring));
    opt_list.add(UCIOption("RazoringFactor", 0, RazoringFactor, 750));

    opt_list.add(UCIOption("NMPruning", NMPruning));
    opt_list.add(UCIOption("NMPruningDepthMin", 1, NMPruningDepthMin, 4));

    opt_list.add(UCIOption("SingularExt", SingularExt));
    opt_list.add(UCIOption("SEDepthMin", 3, SEDepthMin, 7));
    opt_list.add(UCIOption("SEDepthOffset", 2, SEDepthOffset, 6));
    
    opt_list.add(UCIOption("StaticNMP", StaticNMP));
    opt_list.add(UCIOption("StaticNMPDepthMax", 1, StaticNMPDepthMax, 10));
    opt_list.add(UCIOption("StaticNMPFactor", 1, StaticNMPFactor, 500));
    
    opt_list.add(UCIOption("AspMargin", 5, AspMargin, 30));
    
    opt_list.add(UCIOption("UciOverhead", 1, UciOverhead, 2000));
    opt_list.add(UCIOption("UciLog", UciLog));
    
    opt_list.add(UCIOption("FutilityFactor", 10, FutilityFactor, 100));
    
    opt_list.add(UCIOption("EasyMove", EasyMove));
    opt_list.add(UCIOption("EMMargin", 10, EMMargin, 1000));
    opt_list.add(UCIOption("EMShare",   1, EMShare,    99));
    
    if (UciLog) {
        logfile.open(uci_log_filename(), ios::out | ios::app);
        assert(logfile.is_open());
    }
}

void uci_loop()
{
    ttable.init();

    uci_position("position startpos");
   
#if PROFILE >= PROFILE_SOME
    gstats.stimer.start();
#endif

    for (string line; getline(cin, line); ) {
        if (UciLog)
            uci_log<Direction::In>(line);

        if (line.empty() || line[0] == '#')
            continue;

        istringstream iss(line);

        string token;

        iss >> skipws >> token;

        if (token == "dump")
            uci_dump();
        
        else if (token == "go")
            uci_go(line);

        else if (token == "isready") {
            uci_stop();
            uci_send("readyok");
        }
        
        else if (token == "position") {
            uci_stop();
            uci_position(line);
        }

        else if (token == "setoption")
            uci_setoption(line);

        else if (token == "stop")
            uci_stop();

        else if (token == "uci")
            uci_uci();

        else if (token == "ucinewgame") {
            uci_stop();
            uci_ucinewgame();
        }
        
        else if (token == "quit")
            break;

        else
            uci_send("info string unknown command: %s", token.c_str());
    }

    uci_stop();

#if PROFILE >= PROFILE_SOME
    gstats.stimer.stop();
#endif
}

void uci_go(const string& s)
{
    assert(!Searching);

    Tokenizer fields(s);

    assert(fields.size() > 0);

    UciOverhead = opt_list.get("UciOverhead").spin_value();

    sl = SearchLimits();

    i64 time[2] = { };
    i64 inc[2] = { };

    for (size_t i = 1; i < fields.size(); i++) {
        string token = fields[i];

        if (token == "wtime")
            time[White] = stoll(fields[++i]);
        else if (token == "btime")
            time[Black] = stoll(fields[++i]);
        else if (token == "winc")
            inc[White] = stoll(fields[++i]);
        else if (token == "binc")
            inc[Black] = stoll(fields[++i]);
        else if (token == "movestogo")
            sl.movestogo = stoi(fields[++i]);
        else if (token == "depth")
            sl.depth = stoi(fields[++i]);
        else if (token == "nodes")
            sl.nodes = stoi(fields[++i]);
        else if (token == "movetime")
            sl.move_time = stoll(fields[++i]);
        else if (token == "infinite")
            sl.infinite = true;
        else
            assert(false);
    }

    sl.time = time[si.pos.side()];
    sl.inc = inc[si.pos.side()];

    if (sl.move_time)
        sl.move_time = max(i64(sl.move_time - UciOverhead), i64(1));

    else if (sl.time || sl.inc) {
        double mtg = sl.movestogo ? clamp(sl.movestogo, 1, 50) : 50;
        double rem = sl.time + (mtg - 1) * sl.inc - (mtg + 2) * UciOverhead;
        double tpm = max(rem / mtg, 1.0);

        double x1 = max(double(sl.time - UciOverhead), 1.0);
        double x2 = max(rem * 0.9, 1.0);
        double bound = min(x1, x2);

        double f1 = sl.movestogo ?  1.75 : 2.25;
        double f2 = sl.movestogo ? 10.50 : 9.00;

        double xtime = clamp(tpm * f1, 1.0, bound);
        double ptime = clamp(tpm * f2, 1.0, bound);

        sl.max_time   = i64(xtime + 0.5);
        sl.panic_time = i64(ptime + 0.5);
    }

    si.timer.start();

    sthread = thread(search_start);
}

void uci_send(const char format[], ...)
{
    va_list arg_list;
    char buf[4096];

    va_start(arg_list, format);
    vsprintf(buf, format, arg_list);
    va_end(arg_list);

    fprintf(stdout, "%s\n", buf);

    if (UciLog)
        uci_log<Direction::Out>(buf);
}

string uci_score(int score)
{
    ostringstream oss;

    if (score_is_mate(score)) {
        int count;

        if (score > 0)
            count = (ScoreMate - score + 1) / 2;
        else
            count = -(ScoreMate + score + 1) / 2;

        oss << "mate " << count;
    }
    else
        oss << "cp " << score;

    return oss.str();
}

void uci_position(const string& s)
{
    Tokenizer fields(s);

    string fen;

    size_t index = 2;

    if (fields[1] == "startpos")
        fen = Position::StartPos;
    else if (fields[1] == "fen") {
        fen = fields[2];

        for (index++; index < fields.size() && fields[index] != "moves"; index++) {
            fen += ' ';
            fen += fields[index];
        }
    }
    else
        assert(false);
    
    vector<string> moves;

    if (index < fields.size() && fields[index] == "moves")
        moves = fields.subtok(index + 1);

    si = SearchInfo(Position(fen));

    mstack.clear();
    mstack.add(MoveNone);
    mstack.add(MoveNone);

    kstack.clear();
    kstack.add(si.pos.key());

    for (auto m : moves) {
        Move move = si.pos.note_move(m);

        UndoMove undo;

        si.pos.make_move(move, undo);
    }
}

void uci_setoption(const string& s)
{
    Tokenizer fields(s);

    string name = fields[2];
    string value;

    size_t i = 3;

    while (i < fields.size() && fields[i] != "value") {
        name += ' ';
        name += fields[i++];
    }

    if (!opt_list.has_opt(name)) {
        uci_send("info string unknown option: %s", name.c_str());
        return;
    }

    if (i < fields.size() && fields[i] == "value")
        value = fields[i + 1];

    UCIOption& opt = opt_list.get(name);

    if (opt.get_type() != UCIOption::Button) {
        opt.set_value(value);
            
        if (name == "Hash") {
            assert(!Searching);

            ttable = TT(opt.spin_value());
            ttable.init();
        }
        else if (name == "UciLog") {
            UciLog = opt.check_value();

            if (!UciLog && logfile.is_open())
                logfile.close();
            else if (UciLog && !logfile.is_open())
                logfile.open(uci_log_filename(), ios::out | ios::app);
        }
    }
    // Button
    else {
        if (name == "Clear Hash")
            ttable.reset();
    }
}

void uci_uci()
{
    uci_send("id name Cadie %s", CADIE_VERSION);
    uci_send("id author Martin Wyngaarden");

    for (size_t i = 0; i < opt_list.count(); i++)
        cout << opt_list.get(i).to_option() << endl;

    uci_send("uciok");
}

void uci_stop()
{
    if (!Searching) {
        assert(!sthread.joinable());
        assert(!StopRequest);
        return;
    }
        
    assert(sthread.joinable());

    StopRequest = true;

    sthread.join();
    
    Searching = false;
    StopRequest = false;
}

void uci_ucinewgame()
{
    search_reset();
    
    si = SearchInfo();
}

void uci_dump()
{
    for (size_t i = 0; i < opt_list.count(); i++)
        if (opt_list.get(i).get_type() != UCIOption::Button)
            cout << opt_list.get(i).name() << '=' << opt_list.get(i).get_value() << endl;
}

template <Direction D>
void uci_log(const string& s)
{
    assert(logfile.is_open());
    
    i64 elapsed = Timer::now() - gstats.time_init;

    ostringstream oss;

    oss << "0x" << setfill('0') << setw(16) << hex << this_thread::get_id() << ' '
        << setfill(' ') << setw(16) << dec << right << elapsed
        << (D == Direction::In ? " << " : " >> ") << s << endl;

    logfile << oss.str();
    logfile.flush();
}

string uci_log_filename()
{
    for (size_t i = 0; i < 1000; i++) {
        stringstream oss;

        oss << "uci_" << setfill('0') << setw(5) << i << ".log";

        if (!filesystem::exists(filesystem::path{oss.str()}))
            return oss.str();
    }

    return "uci_default.log";
}
