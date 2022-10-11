#ifndef UCI_H
#define UCI_H

#include <string>
#include "string.h"
#include "types.h"

struct GlobalStats {
    i64 nodes_search  = 0;
    i64 nodes_qsearch = 0;

    i64 time_begin    = 0;
    i64 time_end      = 0;
    i64 time_eval     = 0;
    i64 time_gen      = 0;

    i64 moves_made    = 0;
    i64 evals_made    = 0;
};

extern GlobalStats global_stats;

constexpr int UciTimeBuffer = 10;

void uci_init();
void uci_loop();

void uci_parse(const std::string s);
void uci_send(const char format[], ...);
void uci_debug(bool in, std::string s);

std::string uci_score(int score);

extern bool UseTransTable;
extern bool KillerMoves;

extern bool NMPruning;
extern int NMPruningDepthMin;

extern bool SingularExt;

extern bool StaticNMP;
extern int StaticNMPDepthMax;
extern int StaticNMPFactor;

extern bool Razoring;
extern int RazoringMargin;

extern int QSearchMargin;
extern int AspMargin;
extern int TempoBonus;

extern bool UseLazyEval;
extern int LazyMargin;

extern int Y1;
extern int Y2;
extern int Y3;

extern int Z0;
extern int Z1;
extern int Z2;
extern int Z3;
extern int Z4;
extern int Z5;
extern int Z6;
extern int Z7;
extern int Z8;
extern int Z9;
extern int Z10;
extern int Z11;
extern int Z12;

#endif
