#ifndef UCI_H
#define UCI_H

#include <limits>
#include <string>
#include "string.h"
#include "timer.h"
#include "types.h"
#include "uciopt.h"

struct GlobalStats {
    i64 nodes_search    = 0;
    i64 nodes_qsearch   = 0;

    Timer stimer;
    Timer iotimer;
    
    i64 time_eval_ns    = 0;
    i64 time_gen_ns     = 0;
    i64 cycles_eval     = 0;
    i64 cycles_gen      = 0;

    i64 moves_count     = 0;
    i64 evals_count     = 0;

    int depth_min       = std::numeric_limits<int>::max();
    int depth_max       = std::numeric_limits<int>::min();
    int depth_sum       = 0;

    i64 nodes_min       = std::numeric_limits<i64>::max();
    i64 nodes_max       = std::numeric_limits<i64>::min();
    
    i64 time_min        = std::numeric_limits<i64>::max();
    i64 time_max        = std::numeric_limits<i64>::min();
    
    i64 nps_min         = std::numeric_limits<i64>::max();
    i64 nps_max         = std::numeric_limits<i64>::min();

    int unfinished      = 0;
    int stalemate       = 0;
    int checkmate       = 0;

    bool benchmarking   = false;
    bool exc            = true;
    size_t num          = 0;
};

extern GlobalStats gstats;

extern UCIOptionList opt_list;

void uci_init();
void uci_loop();

void uci_send(const char format[], ...);

std::string uci_score(int score);

extern bool UseTransTable;
extern bool KillerMoves;

extern bool NMPruning;
extern int NMPruningDepthMin;

extern bool SingularExt;
extern int SEMovesMax;
extern int SEDepthMin;
extern int SEDepthOffset;

extern bool StaticNMP;
extern int StaticNMPDepthMax;
extern int StaticNMPFactor;

extern bool DeltaPruning;
extern int DPMargin;

extern bool Razoring;
extern int RazoringMargin;

extern int QSearchMargin;
extern int AspMargin;
extern int TempoBonus;

extern bool UseLazyEval;
extern int LazyMargin;

extern int UciTimeBuffer;
extern int UciNodesBuffer;
extern bool UciLog;

extern int FutilityFactor;
extern int FutilityDepthMax;

extern int Z0;
extern int Z1;
extern int Z2;
extern int Z3;
extern int Z4;
extern int Z5;
extern int Z7;
extern int Z8;
extern int Z9;
extern int Z10;

#endif
