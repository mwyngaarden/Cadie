#ifndef SEARCH_H
#define SEARCH_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include <csetjmp>
#include "eval.h"
#include "move.h"
#include "pos.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "uciopt.h"
#include "util.h"

extern std::thread search_thread;

extern KeyStack  key_stack;
extern MoveStack move_stack;

extern std::atomic_bool Stop;

struct Node {
    const Position& pos;

    const int ply;
    const int depth;
    const int alpha;
    const int beta;
    const bool pv_node;
    const bool root;
    const int side;
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

    Node(const Position& pos_, int ply_, int d, int a, int b, Move sm_ = MoveNone) :
        pos(pos_), 
        ply(ply_), 
        depth(d), 
        alpha(a), 
        beta(b), 
        pv_node(b > a + 1),
        root(ply_ == 0), 
        side(pos.side()), 
        in_check(pos.in_check()),
        tt_key(pos.key() ^ sm_),
        sm(sm_)
    {
    }
};

struct SearchLimits {
    int depth;

    bool infinite;

    i64 nodes;
    i64 time;
    i64 time_min;
    i64 time_max;
    i64 inc;
    i64 movetime;
    i64 begin;

    int movestogo;

    void reset()
    {
        depth       = 0;

        infinite    = false;

        nodes       = 0;
        time        = 0;
        time_min    = 0;
        time_max    = 0;
        inc         = 0;
        movetime    = 0;
        begin       = 0;

        movestogo   = 0;
    }

    double time_usage_percent() const
    {
        assert(time_limited());

        const double dur = Util::now() - begin;

        return dur / max_time();
    }

    i64 max_time() const
    {
        assert(time_limited());

        return std::max(movetime, time_max);
    }

    bool time_limited() const
    {
        return time > 0 || movetime > 0;
    }

    std::string dump() const
    {
        std::ostringstream oss;

        oss << "depth = " << depth << std::endl;

        oss << "infinite = " << infinite << std::endl;
        
        oss << "nodes = " << nodes << std::endl;
        oss << "time = " << time << std::endl;
        oss << "time_min = " << time_min << std::endl;
        oss << "time_max = " << time_max << std::endl;
        oss << "inc = " << inc << std::endl;
        oss << "movetime = " << movetime << std::endl;
        oss << "begin = " << begin << std::endl;

        oss << "movestogo = " << movestogo << std::endl;

        return oss.str();
    }
};

extern SearchLimits search_limits;

struct SearchInfo {

    static constexpr i64 NodesCountdown = 1 << 11;

    jmp_buf jump_buffer; 

    int depth_max;
    int seldepth_max;
    int score_best;

    i64 nodes;
    i64 nodes_countdown;

    Move move_best;

    int stability;

    UCIOptionList opt_list;

    Position pos;

    void reset()
    {
        depth_max       = 0;
        seldepth_max    = 0;
        score_best      = ScoreNone;

        nodes           = 0;
        nodes_countdown = NodesCountdown;

        move_best       = MoveNone;

        stability       = 0;

        opt_list        = UCIOptionList();

        pos             = Position(Position::StartPos);
    }

    void update(int depth, int score, PV& pv, bool incomplete);

    void uci_bestmove() const;

    void check();
};

extern SearchInfo search_info;

void search_init();
void search_go();
void search_stop();
void search_reset();
void search_start();

int search_iter(Position& pos, int depth_max, PV& pv);

void search_root();

bool key_stack_rep(const Position& pos);

int mate_in(int ply);
int mated_in(int ply);

bool score_is_mate(int score);

#endif
