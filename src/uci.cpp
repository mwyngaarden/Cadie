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

fstream dfile;

GlobalStats gstats;

enum class Direction { In, Out };

static void uci_go          (const string& s);
static void uci_position    (const string& s);
static void uci_setoption   (const string& s);
static void uci_ucinewgame  ();
static void uci_stop        ();
static void uci_uci         ();
static void uci_dump        ();

static string uci_debug_filename();

template <Direction D>
static void uci_log         (const string& s);

// UCI options with default values

bool UseTransTable        =  true;
bool KillerMoves          =  true;

bool NMPruning            =  true;
int  NMPruningDepthMin    =     2;

bool SingularExt          =  true;
int  SEMovesMax           =     5;
int  SEDepthMin           =     6;
int  SEDepthOffset        =     4;

bool StaticNMP            =  true;
int  StaticNMPDepthMax    =     7;
int  StaticNMPFactor      =   100;

bool DeltaPruning         =  true;
int DPMargin              =   200;

bool Razoring             =  true;
int  RazoringMargin       =   500;

int  QSearchMargin        =   250;
int  AspMargin            =    15;
int  TempoBonus           =    15;

bool UseLazyEval          =  true;
int  LazyMargin           =   900;

int  UciTimeBuffer        =    15;
int  UciNodesBuffer       =   500;
bool UciLog               = false;

int  FutilityFactor       =    50;
int  FutilityDepthMax     =     4; // 3,5

int  Z0                   =     1; // 1,3
int  Z1                   =     3; // 2,4 // 3
int  Z2                   =     1; // 0,2
int  Z3                   =     3; // 2,4
int  Z4                   =     3; // 2,4
int  Z5                   =     3; // 2,4
int  Z7                   =     3; // 1,3
int  Z8                   =     6; // 5,7
int  Z9                   =     3; // 3,5
int  Z10                  =     2; // 0,2

UCIOptionList opt_list;

void uci_init()
{
    cout << "Cadie " << CADIE_VERSION << " (" << CADIE_DATE << ' ' << CADIE_TIME << ") by Martin Wyngaarden" << endl;
    cout << "Compiled with Debug = " << Debug << " ProfileLevel = " << static_cast<std::underlying_type<ProfileLevel>::type>(Profile) << endl;

    opt_list.add(UCIOption("Hash", 1, thtable.size_mb(), 1024));
    opt_list.add(UCIOption("Clear Hash"));

    opt_list.add(UCIOption("Razoring", Razoring));
    opt_list.add(UCIOption("RazoringMargin", 0, RazoringMargin, 750));

    opt_list.add(UCIOption("NMPruning", NMPruning));
    opt_list.add(UCIOption("NMPruningDepthMin", 1, NMPruningDepthMin, 4));

    opt_list.add(UCIOption("KillerMoves", KillerMoves));
    opt_list.add(UCIOption("UseTransTable", UseTransTable));
    
    opt_list.add(UCIOption("SingularExt", SingularExt));
    opt_list.add(UCIOption("SEMovesMax", 3, SEMovesMax, 7));
    opt_list.add(UCIOption("SEDepthMin", 3, SEDepthMin, 7));
    opt_list.add(UCIOption("SEDepthOffset", 2, SEDepthOffset, 6));
    
    opt_list.add(UCIOption("StaticNMP", StaticNMP));
    opt_list.add(UCIOption("StaticNMPDepthMax", 1, StaticNMPDepthMax, 10));
    opt_list.add(UCIOption("StaticNMPFactor", 1, StaticNMPFactor, 500));
    
    opt_list.add(UCIOption("DeltaPruning", DeltaPruning));
    opt_list.add(UCIOption("DPMargin", 50, DPMargin, 500));

    opt_list.add(UCIOption("QSearchMargin", 0, QSearchMargin, 500));
    opt_list.add(UCIOption("AspMargin", 5, AspMargin, 30));
    opt_list.add(UCIOption("TempoBonus", 0, TempoBonus, 50));
    
    opt_list.add(UCIOption("UseLazyEval", UseLazyEval));
    opt_list.add(UCIOption("LazyMargin", 50, LazyMargin, 1000));
    
    opt_list.add(UCIOption("UciTimeBuffer", 1, UciTimeBuffer, 200));
    opt_list.add(UCIOption("UciNodesBuffer", 1, UciNodesBuffer, 100000));
    
    opt_list.add(UCIOption("FutilityFactor", 10, FutilityFactor, 100));
    opt_list.add(UCIOption("FutilityDepthMax", 0, FutilityDepthMax,  10));
    
    opt_list.add(UCIOption("Z0",      0,   Z0,  10));
    opt_list.add(UCIOption("Z1",      0,   Z1,  10));
    opt_list.add(UCIOption("Z2",      0,   Z2,  10));
    opt_list.add(UCIOption("Z3",      0,   Z3,  10));
    opt_list.add(UCIOption("Z4",      0,   Z4,  10));
    opt_list.add(UCIOption("Z5",      0,   Z5,  10));
    opt_list.add(UCIOption("Z7",      0,   Z7,  10));
    opt_list.add(UCIOption("Z8",      0,   Z8,  10));
    opt_list.add(UCIOption("Z9",      0,   Z9,  10));
    opt_list.add(UCIOption("Z10",     0,   Z10, 10));
}

void uci_loop()
{
    if (UciLog) {
        dfile.open(uci_debug_filename(), ios::out | ios::app);
        assert(dfile.is_open());
    }

    thtable.init();

    uci_position("position startpos");
    
    string line;
        
    gstats.stimer.start();
    
    while (getline(cin, line)) {
        uci_log<Direction::In>(line);

        if (line.empty() || line[0] == '#')
            continue;

        istringstream iss(line);

        string token;

        iss >> skipws >> token;

        if (token == "quit")
            break;

        else if (token == "debug")
            continue;

        else if (token == "setoption")
            uci_setoption(line);

        else if (token == "dump")
            uci_dump();

        else if (token == "isready") {
            uci_stop();
            uci_send("readyok");
        }

        else if (token == "uci")
            uci_uci();

        else if (token == "stop")
            uci_stop();

        else if (token == "ucinewgame") {
            uci_stop();
            uci_ucinewgame();
        }

        else if (token == "position") {
            uci_stop();
            uci_position(line);
        }

        else if (token == "go")
            uci_go(line);

        else
            uci_send("info string unknown command: %s", token.c_str());
    }

    uci_stop();

    gstats.stimer.stop();
}

void uci_go(const string& s)
{
    assert(!Searching);

    Tokenizer fields(s);

    assert(fields.size() > 0);

    UciTimeBuffer = opt_list.get("UciTimeBuffer").spin_value();

    slimits = SearchLimits(true);

    const side_t side = sinfo.pos.side();

    for (size_t i = 1; i < fields.size(); i++) {
        const string token = fields[i];

        if (token == "wtime") {
            ++i;
            if (side == White) slimits.time = stoll(fields[i]);
        }
        else if (token == "btime") {
            ++i;
            if (side == Black) slimits.time = stoll(fields[i]);
        }
        else if (token == "winc") {
            ++i;
            if (side == White) slimits.inc = stoll(fields[i]);
        }
        else if (token == "binc") {
            ++i;
            if (side == Black) slimits.inc = stoll(fields[i]);
        }
        else if (token == "movestogo")
            slimits.movestogo = stoi(fields[++i]);
        else if (token == "depth")
            slimits.depth = stoi(fields[++i]);
        else if (token == "nodes")
            slimits.nodes = stoi(fields[++i]);
        else if (token == "movetime")
            slimits.movetime = stoll(fields[++i]);
        else if (token == "infinite")
            slimits.infinite = true;
        else
            assert(false);
    }

    if (slimits.movetime)
        slimits.movetime -= UciTimeBuffer;
    
    else if (slimits.time || slimits.inc) {
        const int movestogo = slimits.movestogo ? slimits.movestogo : 30;
        const i64 remaining = slimits.time + (movestogo - 1) * slimits.inc;

        i64 ntime = max(min((i64)round(remaining / movestogo * 0.40), slimits.time - UciTimeBuffer), (i64)0);
        i64 xtime = max(min((i64)round(remaining / movestogo * 2.39), slimits.time - UciTimeBuffer), (i64)1);

        assert(ntime <= xtime);

        slimits.time_min = ntime;
        slimits.time_max = xtime;
    }

    search_thread = thread(search_start);

    while (!Searching) this_thread::sleep_for(chrono::microseconds(100));
}

void uci_send(const char format[], ...)
{
    gstats.iotimer.start();

    va_list arg_list;
    char buf[4096];

    va_start(arg_list, format);
    vsprintf(buf, format, arg_list);
    va_end(arg_list);

    fprintf(stdout, "%s\n", buf);

    uci_log<Direction::Out>(buf);
    
    gstats.iotimer.stop();
    gstats.iotimer.accrue();
}

string uci_score(int score)
{
    ostringstream oss;

    // TODO tune
    // if (score == -1 || score == 1) score = 0;

    if (score_is_mate(score)) {
        int count;

        if (score > 0)
            count = (ScoreMate - score + 1) / 2;
        else
            count = -(score + ScoreMate + 1) / 2;

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

    sinfo = SearchInfo(Position(fen));

    move_stack.clear();
    move_stack.add(MoveNone);
    move_stack.add(MoveNone);

    key_stack.clear();
    key_stack.add(sinfo.pos.key());

    for (auto m : moves) {
        Move move = sinfo.pos.note_move(m);

        MoveUndo undo;
        sinfo.pos.make_move(move, undo);

        key_stack.add(sinfo.pos.key());
        move_stack.add(move);
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
            int mb = opt.clamp(stoi(value));
            
            thtable = TransTable(mb);
            thtable.init();
        }
        else if (name == "UciLog") {
            UciLog = opt.check_value();

            if (!UciLog && dfile.is_open())
                dfile.close();
            else if (UciLog && !dfile.is_open()) {
                dfile.open(uci_debug_filename(), ios::out | ios::app);
                assert(dfile.is_open());
            }
        }
    }
    else {

        if (name == "Clear Hash")
            thtable.reset();
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
        assert(!search_thread.joinable());
        assert(!StopRequest);
        return;
    }
        
    assert(search_thread.joinable());

    StopRequest = true;

    search_thread.join();
    
    Searching = false;
    StopRequest = false;
}

void uci_ucinewgame()
{
    search_reset();
    
    sinfo = SearchInfo();
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
    if (!UciLog) return;
                
    assert(dfile.is_open());
    assert(gstats.stimer.status() == Timer::Started);

    i64 elapsed = gstats.stimer.elapsed_time();

    ostringstream oss;

    oss << "0x" << setfill('0') << setw(16) << hex << this_thread::get_id() << ' '
        << setfill(' ') << setw(16) << dec << right << elapsed
        << (D == Direction::In ? " << " : " >> ") << s << endl;

    dfile << oss.str();
    dfile.flush();
}

string uci_debug_filename()
{
    string filename;

    for (size_t i = 0; i < 100 && filename.empty(); i++) {
        stringstream oss;

        oss << "debug_" << setfill('0') << setw(4) << i << ".log";

        if (!filesystem::exists(filesystem::path { oss.str() }))
            filename = oss.str();
    }

    assert(!filename.empty());
    return filename;
}
