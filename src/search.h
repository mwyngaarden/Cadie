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

    const int oalfa;
    const int ply;
    const int depth;
    const bool pv_node;
    const bool root;
    
    bool futile = false;
    bool improving;

    int legals = 0;

    i16 eval = -ScoreMate;

    Entry entry = Entry::init();

    const u64 key;
    bool hit = false;

    Move bm = MoveNone;
    Move sm;

    i16 bs = -ScoreMate;
    i16 fs = -ScoreMate;

    Node(const Position& pos_, int alfa_, int beta_, int ply_, int depth_, Move sm_ = MoveNone) :
        pos(pos_), 
        oalfa(alfa_),
        ply(ply_), 
        depth(depth_), 
        pv_node(beta_ > oalfa + 1),
        root(ply == 0), 
        key(pos.key() ^ sm_),
        sm(sm_)
    {
    }
};

struct SearchLimits {
    int depth       = 0;

    bool infinite   = false;

    i64 nodes       = 0;
    i64 time        = 0;
    i64 time_min    = 0;
    i64 time_max    = 0;
    i64 time_panic  = 0;
    i64 inc         = 0;
    i64 movetime    = 0;

    int movestogo   = 0;

    bool time_limited() const { return time || movetime; }
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

    int cm_num          = 0; // curr move number

    int depth_max       = 0;
    int depth_sel       = 0;

    Move bm             = MoveNone; // best move
    Move cm             = MoveNone; // curr move
    Move em             = MoveNone; // easy move

    Position pos;

    Timer timer;

    SearchInfo() = default;

    SearchInfo(Position p) : pos(p) { }
    
    void update(int depth, int score, PV& pv, bool complete = true);
    
    double time_share_min(const SearchLimits &slimits) const
    {
        assert(slimits.time);
        return static_cast<double>(timer.elapsed_time()) / slimits.time_min;
    }

    double time_share_max(const SearchLimits &slimits) const
    {
        assert(slimits.time);
        return static_cast<double>(timer.elapsed_time()) / slimits.time_max;
    }

    bool is_stable() const { return !fail_highs && !score_drop; }
    
    void uci_report(i64 dur) const;
    void uci_bestmove() const;

    double factor() const;

    void checkup();
};

extern SearchInfo sinfo;
extern SearchLimits slimits;

void search_init();
void search_go();
void search_stop();
void search_reset();
void search_start();
void search_root();

bool kstack_rep(const Position& pos);

int mate_in(int ply);
int mated_in(int ply);

bool score_is_mate(int score);

#endif
