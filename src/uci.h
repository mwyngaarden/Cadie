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
    i64 nodes_search    = 0;
    i64 nodes_qsearch   = 0;

    Timer stimer;
    
    i64 time_eval_ns    = 0;
    i64 cycles_eval     = 0;
    
    i64 time_gen_ns    = 0;

    i64 calls_gen[std::size_t(GenMode::Count)]  = { 0 };
    i64 cycles_gen[std::size_t(GenMode::Count)] = { 0 };

    i64 moves_count     = 0;
    i64 evals_count     = 0;

    int depth_min       = std::numeric_limits<int>::max();
    int depth_max       = std::numeric_limits<int>::min();
    int depth_sum       = 0;
    int seldepth_min    = std::numeric_limits<int>::max();
    int seldepth_max    = std::numeric_limits<int>::min();
    int seldepth_sum    = 0;
    int lmoves_min      = std::numeric_limits<int>::max();
    int lmoves_max      = std::numeric_limits<int>::min();
    int lmoves_sum      = 0;
    int amoves_max      = 0;

    int bm_updates      = 0;
    int bm_stable       = 0;

    i64 nodes_min       = std::numeric_limits<i64>::max();
    i64 nodes_max       = std::numeric_limits<i64>::min();
    
    i64 time_min        = std::numeric_limits<i64>::max();
    i64 time_max        = std::numeric_limits<i64>::min();
    
    i64 nps_min         = std::numeric_limits<i64>::max();
    i64 nps_max         = std::numeric_limits<i64>::min();

    int normal          = 0;
    int stalemate       = 0;
    int checkmate       = 0;

    bool exc            = true;
    std::size_t num     = 0;

    std::size_t ctests  = 0;
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
extern int SEMovesMax;
extern int SEDepthMin;
extern int SEDepthOffset;

extern bool StaticNMP;
extern int StaticNMPDepthMax;
extern int StaticNMPFactor;

extern bool DeltaPruning;
extern int DPMargin;

extern bool Razoring;
extern int RazoringFactor;

extern int AspMargin;
extern int TempoBonus;

extern int MoveOverhead;

extern int FutilityFactor;

extern bool EasyMove;
extern int EMMargin;
extern int EMShare;

#endif
