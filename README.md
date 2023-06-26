## Overview

Cadie is a stand-alone UCI chess engine written in platform independent C++. It is also the successor to my first UCI chess engine, Keele. Building requires a compiler supporting at least C++20. Being a stand-alone engine, Cadie requires a UCI compatible GUI to run. Compatible GUIs include Arena, ChessBase, and SCID, to name only a few.

## Specifications and features
- 16x12 Mailbox Board Representation
- Static Exchange Evaluation (SEE)
- Null Move Pruning (NMP)
- Razoring
- Transposition Tables
- Zobrist Hashing
- Enhanced Alpha-Beta Search - Principal Variation Search (PVS)
- Aspiration Windows
- Late Move Reductions (LMR)
- Evaluation Pruning
- History Scoring and Reductions
- Killer Moves
- Evaluation Tuning - Cross Entropy Method (CEM)

## Upcoming features

- Ponder
- MultiPV
- PolyGlot opening book support
- Fischer Random Chess - Chess960

## Compiling

Cadie can be compiled at the command line using the make command.  There are currently no build scripts or project/solution files for compilation on Windows.

## Thanks

The fine people at https://www.TalkChess.com for their advice and https://www.ChessProgramming.org for their documentation.   
Andrew Grant (https://github.com/AndyGrant/Ethereal) for transposition table features, evaluation features and ideas, and his positional suite for evaluation tuning.   
Fabien Letouzey (https://www.fruitchess.com) for search and evaluation features.   
Marco Belli (https://github.com/elcabesa/vajolet) for his huge volume of chess positions that were invaluable for move generation debugging.  

