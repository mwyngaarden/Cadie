#ifndef SEARCH_H
#define SEARCH_H

#include <atomic>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include <csetjmp>
#include "eval.h"
#include "move.h"
#include "pos.h"
#include "timer.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "uciopt.h"

extern std::thread search_thread;

extern KeyStack  key_stack;
extern MoveStack move_stack;

extern std::atomic_bool Searching;
extern std::atomic_bool StopRequest;

struct Node {
    const Position& pos;

    const int alpha;
    const int beta;
    const int ply;
    const int depth;
    const bool pv_node;
    const bool root;
    const side_t side;
    const bool in_check;
    
    bool cut_node;
    bool futile = false;

    i16 score = -ScoreMate;
    i16 eval = ScoreNone;

    const u64 tt_key;
    i8 tt_depth;
    u8 tt_bound;
    i16 tt_score = -ScoreMate;
    i16 tt_eval = ScoreNone;
    Move tt_move = MoveNone;

    Move bm = MoveNone;
    Move sm;

    i16 bs = -ScoreMate;

    Node(const Position& pos_, int alpha_, int beta_, int ply_, int depth_, Move sm_ = MoveNone) :
        pos(pos_), 
        alpha(alpha_), 
        beta(beta_), 
        ply(ply_), 
        depth(depth_), 
        pv_node(beta > alpha + 1),
        root(ply == 0), 
        side(pos.side()), 
        in_check(pos.in_check()),
        tt_key(pos.key() ^ sm_),
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
    i64 inc         = 0;
    i64 movetime    = 0;

    int movestogo   = 0;

    Timer timer;

    SearchLimits(bool s = false)
    {
        timer.start(s);
    }

    bool using_tm() const { return time || inc; }

    double time_elapsed() const
    {
        assert(using_tm());

        return static_cast<double>(timer.elapsed_time()) / time_max;
    }

    std::string dump() const
    {
        std::ostringstream oss;

        oss << "depth = " << depth << std::endl
            << "infinite = " << infinite << std::endl
            << "nodes = " << nodes << std::endl
            << "time = " << time << std::endl
            << "time_min = " << time_min << std::endl
            << "time_max = " << time_max << std::endl
            << "inc = " << inc << std::endl
            << "movetime = " << movetime << std::endl
            << "movestogo = " << movestogo << std::endl;

        return oss.str();
    }

    int depth_min() const { return using_tm() ? 5 : 0; }
};

struct SearchInfo {
    jmp_buf jump_buffer; 

    int depth_max       = 0;
    int seldepth        = 0;
    int score_best      = ScoreNone;

    i64 nodes           = 0;
    i64 nodes_countdown = 0;

    Move move_best      = MoveNone;

    int stability       = 0;

    Position pos;

    SearchInfo() = default;
    SearchInfo(Position p) : pos(p) { }

    void update(int depth, int score, PV& pv, bool complete);

    void uci_bestmove() const;

    void check();
};

extern SearchInfo sinfo;
extern SearchLimits slimits;

extern Move Killers[2][PliesMax][2];
extern i16 History[2][128][128];

void search_init();
void search_go();
void search_stop();
void search_reset();
void search_start();

void search_root();

bool key_stack_rep(const Position& pos);

int mate_in(int ply);
int mated_in(int ply);

bool score_is_mate(int score);

#endif
