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

extern std::atomic_bool Searching;
extern std::atomic_bool StopRequest;

struct Node {
    const Position& pos;

    PV pv;

    const int orig_alfa;
    const bool pv_node;

    bool improving;

    int legals = 0;

    i16 eval = -ScoreMate;

    bool tthit;
    Entry tte;

    Move best_move = Move::None();
    Move ext_move  = Move::None();

    i16 best_score = -ScoreMate;

    Node(const Position& p, int alfa, int beta) :
        pos(p),
        orig_alfa(alfa),
        pv_node(beta - alfa > 1),
        tte(Move::None())
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
    i64 opt_time    = 0;
    i64 max_time    = 0;

    int movestogo   = 0;

    bool time_managed() const { return time || inc; }
    bool time_limited() const { return time || inc || move_time; }
};

struct SearchInfo {
    Position pos;

    bool fail_low;

    i64 cnodes;
    i64 tnodes;

    i64 rep_time;

    int bm_stable;
    int bm_updates;

    int score_prev;
    bool score_drop;

    int curr_move_num;

    int max_depth;
    int sel_depth;
    int root_depth;

    bool singular;

    Move best_move;
    Move curr_move;

    Timer timer;

    SearchInfo() = default;
    SearchInfo(Position p) : pos(p) { }

    void update(int depth, int score, PV& pv, bool complete = true);

    double time_usage(const SearchLimits &sl) const
    {
        return double(timer.elapsed_time()) / sl.opt_time;
    }

    void uci_info(i64 dur) const;
    void uci_bestmove() const;

    void checkup();

    void reset()
    {
        fail_low        = false;

        cnodes          = 0;
        tnodes          = 0;

        rep_time        = 0;

        bm_stable       = 0;
        bm_updates      = 0;

        score_prev      = 0;
        score_drop      = false;

        curr_move_num   = 0;

        max_depth       = 0;
        sel_depth       = 0;
        root_depth      = 0;

        singular        = false;

        best_move       = Move::None();
        curr_move       = Move::None();

        timer           = Timer();
    }
};

extern SearchInfo si;
extern SearchLimits sl;

void search_init();
void search_reset();
void search_start();

constexpr int mate_in(int ply) { return ScoreMate - ply; }
constexpr int mated_in(int ply) { return ply - ScoreMate; }

bool score_is_mate(int score);
bool score_is_eval(int score);

#endif
