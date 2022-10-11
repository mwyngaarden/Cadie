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
#include "tt.h"
#include "uci.h"
#include "uciopt.h"
#include "util.h"
using namespace std;

fstream debug_file;

GlobalStats global_stats;

static void uci_go          (const string& s);
static void uci_position    (const string& s);
static void uci_setoption   (const string& s);

static void uci_isready();
static void uci_ucinewgame();
static void uci_stop();
static void uci_uci();
static void uci_dump();

// UCI options

bool UseTransTable        =  true;
bool KillerMoves          =  true;

bool NMPruning            =  true;
int  NMPruningDepthMin    =     2; // try 1!

bool SingularExt          =  true;

bool StaticNMP            =  true;
int  StaticNMPDepthMax    =     6;
int  StaticNMPFactor      =    80;

bool Razoring             =  true;
int  RazoringMargin       =   500;

int  QSearchMargin        =   250;
int  AspMargin            =    15;
int  TempoBonus           =    15;

bool UseLazyEval          =  true;
int  LazyMargin           =   900;

int  Y1                   =     5;
int  Y2                   =     6;
int  Y3                   =     4;

int  Z0                   =     1; // 1,3
int  Z1                   =     3; // 2,4 // 3
int  Z2                   =     1; // 0,2
int  Z3                   =     3; // 2,4
int  Z4                   =     3; // 2,4
int  Z5                   =     3; // 2,4
int  Z6                   =     4; // 3,5
int  Z7                   =     3; // 1,3
int  Z8                   =     6; // 5,7
int  Z9                   =     3; // 3,5
int  Z10                  =     2; // 0,2
int  Z11                  =    50;
int  Z12                  =   300; // 150 looked even, 175 looked even, 200 even

UCIOptionList opt_list;

void uci_init()
{
    cout << "Cadie " << CADIE_VERSION << " (" << CADIE_DATE << ' ' << CADIE_TIME << ") by " << CADIE_AUTHOR << endl;

    global_stats.time_begin = Util::now();

    opt_list.add(UCIOption("Hash", 1, thtable.size_mb(), 512));
    opt_list.add(UCIOption("Clear Hash"));

    opt_list.add(UCIOption("Razoring", Razoring));
    opt_list.add(UCIOption("RazoringMargin", 0, RazoringMargin, 750));

    opt_list.add(UCIOption("NMPruning", NMPruning));
    opt_list.add(UCIOption("NMPruningDepthMin", 1, NMPruningDepthMin, 4));

    opt_list.add(UCIOption("KillerMoves", KillerMoves));
    opt_list.add(UCIOption("UseTransTable", UseTransTable));
    
    opt_list.add(UCIOption("SingularExt", SingularExt));
    
    opt_list.add(UCIOption("StaticNMP", StaticNMP));
    opt_list.add(UCIOption("StaticNMPDepthMax", 1, StaticNMPDepthMax, 10));
    opt_list.add(UCIOption("StaticNMPFactor", 1, StaticNMPFactor, 500));

    opt_list.add(UCIOption("QSearchMargin", 0, QSearchMargin, 500));
    opt_list.add(UCIOption("AspMargin", 5, AspMargin, 30));
    opt_list.add(UCIOption("TempoBonus", 0, TempoBonus, 50));
    
    opt_list.add(UCIOption("UseLazyEval", UseLazyEval));
    opt_list.add(UCIOption("LazyMargin", 50, LazyMargin, 1000));
    
    opt_list.add(UCIOption("Y1",      3,   Y1,   7));
    opt_list.add(UCIOption("Y2",      3,   Y2,   7));
    opt_list.add(UCIOption("Y3",      2,   Y3,   6));
    
    opt_list.add(UCIOption("Z0",      0,   Z0,  10));
    opt_list.add(UCIOption("Z1",      0,   Z1,  10));
    opt_list.add(UCIOption("Z2",      0,   Z2,  10));
    opt_list.add(UCIOption("Z3",      0,   Z3,  10));
    opt_list.add(UCIOption("Z4",      0,   Z4,  10));
    opt_list.add(UCIOption("Z5",      0,   Z5,  10));
    opt_list.add(UCIOption("Z6",      0,   Z6,  10));
    opt_list.add(UCIOption("Z7",      0,   Z7,  10));
    opt_list.add(UCIOption("Z8",      0,   Z8,  10));
    opt_list.add(UCIOption("Z9",      0,   Z9,  10));
    opt_list.add(UCIOption("Z10",     0,   Z10, 10));
    opt_list.add(UCIOption("Z11",    10,   Z11, 100));
    opt_list.add(UCIOption("Z12",    10,   Z12, 500));
}

void uci_loop()
{
    if (DebugLog) {
        debug_file.open("debug.log", ios::out | ios::app);

        assert(debug_file.is_open());
    }

    string line;

    while (getline(cin, line)) {
        if (DebugLog)
            uci_debug(true, line);

        if (line.empty())
            continue;

        if (line[0] == '#')
            continue;

        istringstream iss(line);

        string token;

        iss >> skipws >> token;

        if (token.empty())
            continue;

        if (token == "quit" || token == "exit")
            break;

        else if (token == "setoption")
            uci_setoption(line);

        else if (token == "dump")
            uci_dump();

        else if (token == "debug")
            continue;

        else if (token == "isready")
            uci_isready();

        else if (token == "uci")
            uci_uci();

        else if (token == "stop")
            uci_stop();

        else if (token == "ucinewgame") {
            if (search_thread.joinable())
                search_thread.join();

            uci_ucinewgame();
        }

        else if (token == "position") {
            if (search_thread.joinable())
                search_thread.join();

            uci_position(line);
        }

        else if (token == "go") {
            if (search_thread.joinable())
                search_thread.join();

            uci_go(line);
        }

        else
            uci_send("info string unknown command: %s", token.c_str());
    }

    // wait?

    if (search_thread.joinable())
        search_thread.join();

    global_stats.time_end = Util::now();

    {
        i64 gdur   = global_stats.time_end - global_stats.time_begin;
        i64 mdur   = global_stats.time_gen;
        i64 edur   = global_stats.time_eval;
        i64 qnodes = global_stats.nodes_qsearch;
        i64 snodes = global_stats.nodes_search;
        i64 tnodes = qnodes + snodes;
        i64 mnodes = global_stats.moves_made;

        uci_send("info string gentime %" PRIi64
                 " gentime %% %f"
                 " nodes %" PRIi64 
                 " mnodes %f time %i"
                 " mnps %f"
                 " eval %" PRIi64
                 " eval %% %f",
                 gdur,
                 100.0 * mdur / gdur,
                 mnodes,
                 mnodes / 1000000.0, 
                 gdur,
                 mnodes / gdur / 1000.0,
                 edur,
                 100.0 * edur / gdur);

        uci_send("info string qnodes %" PRIi64
                 " snodes %" PRIi64 
                 " tnodes %" PRIi64 
                 " qnodes %% %f"
                 " tnps %f",
                 qnodes,
                 snodes,
                 tnodes,
                 100.0 * qnodes / tnodes,
                 tnodes / gdur / 1000.0);
    }
}

void uci_go(const string& s)
{
    Tokenizer fields(s, ' ');

    assert(fields.size() > 0);

    if (search_thread.joinable())
        search_thread.join();

    SearchLimits& limits = search_limits;

    limits.reset();
    limits.begin = Util::now();

    const int side = search_info.pos.side();

    for (size_t i = 1; i < fields.size(); i++) {
        const string token = fields[i];

        if (token == "wtime" && side == White)
            limits.time = stoll(fields[++i]);
        else if (token == "btime" && side == Black)
            limits.time = stoll(fields[++i]);
        else if (token == "winc" && side == White)
            limits.inc = stoll(fields[++i]);
        else if (token == "binc" && side == Black)
            limits.inc = stoll(fields[++i]);
        else if (token == "movestogo")
            limits.movestogo = stoi(fields[++i]);
        else if (token == "depth")
            limits.depth = stoi(fields[++i]);
        else if (token == "nodes")
            limits.nodes = stoi(fields[++i]);
        else if (token == "movetime")
            limits.movetime = stoi(fields[++i]);
        else if (token == "infinite")
            limits.infinite = true;
    }

    if (limits.movetime)
        limits.movetime -= UciTimeBuffer;
    
    else if (!limits.movetime && (limits.time || limits.inc)) {
        const int movestogo = limits.movestogo ? limits.movestogo : 30;
        const i64 remaining = limits.time + (movestogo - 1) * limits.inc;

        i64 ntime = min((i64)round(remaining / movestogo * 0.40), limits.time - UciTimeBuffer);
        i64 xtime = min((i64)round(remaining / movestogo * 2.39), limits.time - UciTimeBuffer);

        limits.time_min = ntime;
        limits.time_max = xtime;
    }

    search_thread = thread(search_start);
}

void uci_send(const char format[], ...)
{
    va_list arg_list;
    char s[4096];

    va_start(arg_list, format);
    vsprintf(s, format, arg_list);
    va_end(arg_list);

    fprintf(stdout, "%s\n", s);

    if (DebugLog)
        uci_debug(false, string(s));
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
    Tokenizer fields(s, ' ');

    string fen;

    size_t index = 0;

    if (fields[1] == "startpos") {
        fen = Position::StartPos;
        index = 2;
    } 
    else if (fields[1] == "fen") {
        fen = fields[2];

        for (size_t i = 3; i < fields.size(); i++) {
            if (fields[i] == "moves") {
                index = i;
                break;
            }

            fen += ' ';
            fen += fields[i];
        }
    }

    search_info.reset();
    search_info.pos = Position(fen);
    search_info.opt_list = opt_list;

    move_stack.clear();
    move_stack.add(MoveNone);
    move_stack.add(MoveNone);

    key_stack.clear();
    key_stack.add(search_info.pos.key());

    if (index < fields.size() && fields[index] == "moves") {
        MoveUndo undo;

        for (size_t i = index + 1; i < fields.size(); i++) {
            Move move = search_info.pos.note_move(fields[i]);

            search_info.pos.make_move(move, undo);

            // uci_send("info string dump: %s", limits.pos.dump().c_str());

            key_stack.add(search_info.pos.key());
            move_stack.add(move);
        }
    }
}

void uci_isready()
{
    uci_send("readyok");
}

void uci_setoption(const string& s)
{
    Tokenizer fields(s, ' ');

    string name = fields[2];
    string value;

    size_t i = 3;

    while (i < fields.size() && fields[i] != "value") {
        name += ' ';
        name += fields[i++];
    }

    if (!opt_list.has_opt(name))
        return;

    if (i < fields.size() && fields[i] == "value")
        value = fields[i + 1];

    UCIOption& opt = opt_list.get(name);

    if (opt.get_type() != UCIOption::Button) {
        opt.set_value(value);

        if (name == "Hash") {
            int v = opt.clamp(stoi(value));
            
            thtable = TransTable(v);
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
    uci_send("id author %s", CADIE_AUTHOR);

    for (size_t i = 0; i < opt_list.count(); i++)
        cout << opt_list.get(i).to_option() << endl;

    uci_send("uciok");
}

void uci_stop()
{
    if (search_thread.joinable())
        search_thread.join();
}

void uci_ucinewgame()
{
    search_reset();

    search_info.reset();
}

void uci_dump()
{
    for (size_t i = 0; i < opt_list.count(); i++)
        if (opt_list.get(i).get_type() != UCIOption::Button)
            cout << opt_list.get(i).name() << '=' << opt_list.get(i).get_value() << endl;
}

void uci_debug(bool in, string s)
{
    assert(DebugLog);

    ostringstream oss;

    thread::id this_id = this_thread::get_id();

    oss << "0x" << setfill('0') << setw(16) << hex << this_id << ' '
        << dec << setfill(' ') << setw(16) << right << global_stats.time_begin << ' '
        << dec << setw(8) << setfill(' ') << right << Util::now() - global_stats.time_begin << ' ';

    oss << (in ? "<< " : ">> ");
    oss << s;

    debug_file << oss.str() << endl;
    debug_file.flush();
}
