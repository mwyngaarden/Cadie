#ifndef SEARCH_H
#define SEARCH_H

#include <atomic>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include <cstring>
#include "eval.h"
#include "gen.h"
#include "misc.h"
#include "move.h"
#include "pos.h"
#include "timer.h"
#include "tt.h"
#include "uci.h"
#include "uciopt.h"

extern KeyStack  kstack;
extern MoveStack mstack;

extern int Evals[PliesMax];

extern std::atomic_bool Searching;
extern std::atomic_bool StopRequest;

struct Node {
    const Position& pos;

    PV pv;

    const int orig_alfa;
    const int ply;
    const int depth;
    const bool pv_node;
    const bool root_node;
    
    bool futile = false;
    bool improving;

    int legals = 0;

    i16 eval = -ScoreMate;

    Entry entry;

    const u64 key;
    bool hit = false;

    Move best_move;
    Move skip_move;
    Move xtnd_move;

    i16 bs = -ScoreMate;
    i16 fs = -ScoreMate;

    Node(const Position& pos_, int alfa, int beta, int ply_, int depth_, Move skip_move_ = MoveNone) :
        pos(pos_), 
        orig_alfa(alfa),
        ply(ply_), 
        depth(depth_), 
        pv_node(beta != alfa + 1),
        root_node(ply == 0),
        key(pos.key() ^ skip_move_),
        skip_move(skip_move_)
    {
    }
};

struct SearchLimits {
    int depth       = 0;

    bool infinite   = false;

    i64 nodes       = 0;
    i64 time        = 0;
    i64 inc         = 0;
    i64 move_time   = 0;
    i64 max_time    = 0;
    i64 panic_time  = 0;

    int movestogo   = 0;

    bool time_managed() const { return time || inc; }
    bool time_limited() const { return time || inc || move_time; }
};

struct SearchInfo {
    bool initialized    = false;

    bool singular       = false;

    bool fail_low       = false;
    int fail_highs      = 0;

    i64 cnodes          = 0;
    i64 tnodes          = 0;

    i64 time_rep        = 0;

    int bm_stable       = 0;
    int bm_updates      = 0;

    int score_prev      = 0;
    bool score_drop     = false;

    int curr_move_num   = 0;

    int depth_max       = 0;
    int depth_sel       = 0;

    Move best_move;
    Move curr_move;
    Move easy_move;

    Position pos;

    Timer timer;

    SearchInfo() = default;

    SearchInfo(Position pos_) : pos(pos_) { }
    
    void update(int depth, int score, PV& pv, bool complete = true);
    
    double time_usage(const SearchLimits &sl) const
    {
        assert(sl.time || sl.inc);
        return double(timer.elapsed_time()) / sl.max_time;
    }

    bool is_stable() const { return !fail_highs && !score_drop; }
    
    void uci_info(i64 dur) const;
    void uci_bestmove() const;

    void checkup();
};

extern SearchInfo si;
extern SearchLimits sl;

void search_init();
void search_go();
void search_stop();
void search_reset();
void search_start();
void search_root();

int mate_in(int ply);
int mated_in(int ply);

bool score_is_mate(int score);

#endif
