## Overview

Cadie is a stand-alone UCI chess engine written in (mostly) platform independent C++. It is also the successor to my first UCI chess engine, Keele. Building requires a compiler supporting at least C++20. Being a stand-alone engine, Cadie requires a UCI compatible GUI to run. Compatible GUIs include Arena (http://www.playwitharena.de), ChessBase, and SCID, to name only a few.

## Specifications and Features
- Bitboard Board Representation
- Static Exchange Evaluation (SEE)
- Null Move Pruning (NMP)
- Razoring
- Transposition Tables with Zobrist Hashing
- Enhanced Alpha-Beta Search - Principal Variation Search (PVS)
- Aspiration Windows
- Late Move Reductions (LMR)
- Late Move Pruning (LMP)
- Evaluation Pruning
- Singular Extensions
- Quiet/Continuation/Killer/Counter Move History Scoring and Reductions
- Evaluation Tuning - NLopt - Sbplx and Controlled Random Search (CRS)

## Upcoming Features

- Syzygy Endgame Tablebases
- PolyGlot Opening Book Support
- Fischer Random Chess - Chess960
- Ponder
- MultiPV

## Compiling

Cadie can be compiled at the command line using the make command.  There are currently no build scripts or project/solution files for compilation on Windows.

## Thanks

Graham Banks, a co-founder and tester at CCRL (https://en.wikipedia.org/wiki/Computer_chess#Computer_chess_rating_lists)   
Thomas Jahn for his sliding-move-generation bitboard scheme (https://github.com/lithander/Leorik)   
The fine people at https://www.TalkChess.com for their advice and https://www.ChessProgramming.org for their documentation.   
Andrew Grant (https://github.com/AndyGrant/Ethereal) for evaluation features and ideas, and his positional suite for evaluation tuning.   
Fabien Letouzey (https://www.fruitchess.com) for search and evaluation features.   
Marco Belli (https://github.com/elcabesa/vajolet) for his huge volume of chess positions that were invaluable for move generation debugging.  

