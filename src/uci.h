#ifndef UCI_H
#define UCI_H

#include <limits>
#include <string>
#include "gen.h"
#include "misc.h"
#include "string.h"
#include "timer.h"
#include "uciopt.h"

struct GlobalStats {
    i64 time_init       = 0;

    Timer stimer;
    
    i64 time_eval_ns    = 0;
    i64 cycles_eval     = 0;
    
    i64 time_gen_ns    = 0;

    i64 calls_gen[std::size_t(GenMode::Count)]  = { 0 };
    i64 cycles_gen[std::size_t(GenMode::Count)] = { 0 };

    i64 evals_count     = 0;

    int depth_min       = std::numeric_limits<int>::max();
    int depth_max       = std::numeric_limits<int>::min();
    int depth_sum       = 0;
    int seldepth_min    = std::numeric_limits<int>::max();
    int seldepth_max    = std::numeric_limits<int>::min();
    int seldepth_sum    = 0;

    int moves_max       = 0;

    i64 nodes_min       = std::numeric_limits<i64>::max();
    i64 nodes_sum       = 0;
    i64 nodes_max       = std::numeric_limits<i64>::min();
    
    i64 time_min        = std::numeric_limits<i64>::max();
    i64 time_max        = std::numeric_limits<i64>::min();
    
    i64 nps_min         = std::numeric_limits<i64>::max();
    i64 nps_max         = std::numeric_limits<i64>::min();

    int normal          = 0;
    int stalemate       = 0;
    int checkmate       = 0;

    bool exc_mated      = true;
    std::size_t num     = 0;

    std::size_t stests  = 0;
};

extern GlobalStats gstats;

extern UCIOptionList opt_list;

void uci_init();
void uci_loop();

void uci_send(const char format[], ...);
void uci_setoption(const std::string& s);

std::string uci_score(int score);

extern bool NMPruning;
extern int NMPruningDepthMin;

extern bool SingularExt;
extern int SEDepthMin;
extern int SEDepthOffset;

extern bool StaticNMP;
extern int StaticNMPDepthMax;
extern int StaticNMPFactor;

extern bool ProbCut;
extern int ProbCutDepthMin;
extern int ProbCutMargin;

extern int AspMargin;

#endif
