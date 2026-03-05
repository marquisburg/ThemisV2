#ifndef DEFS_H
#define DEFS_H

#include <cstdint>
#include <atomic>
#include <iostream>
#include <string>
#include <vector>

typedef uint64_t U64;

// Board Squares
enum {
  A1,
  B1,
  C1,
  D1,
  E1,
  F1,
  G1,
  H1,
  A2,
  B2,
  C2,
  D2,
  E2,
  F2,
  G2,
  H2,
  A3,
  B3,
  C3,
  D3,
  E3,
  F3,
  G3,
  H3,
  A4,
  B4,
  C4,
  D4,
  E4,
  F4,
  G4,
  H4,
  A5,
  B5,
  C5,
  D5,
  E5,
  F5,
  G5,
  H5,
  A6,
  B6,
  C6,
  D6,
  E6,
  F6,
  G6,
  H6,
  A7,
  B7,
  C7,
  D7,
  E7,
  F7,
  G7,
  H7,
  A8,
  B8,
  C8,
  D8,
  E8,
  F8,
  G8,
  H8,
  NO_SQ
};

// Pieces
enum { wP, wN, wB, wR, wQ, wK, bP, bN, bB, bR, bQ, bK, EMPTY };

// Colors
enum { WHITE, BLACK, BOTH };

// Castling Rights
enum { WKCA = 1, WQCA = 2, BKCA = 4, BQCA = 8 };

struct Move {
  int from;
  int to;
  int piece;
  int capture;
  int promoted;
  // Flags for special moves?
};

class Board {
public:
  U64 pieces[12];     // Bitboards for each piece type
  U64 occupancies[3]; // WHITE, BLACK, BOTH

  int side;
  int enPas;      // Square for en passant
  int castle;     // Castling rights bitmask
  U64 zobristKey; // Position Hash

  int squares[64]; // Mailbox representation for O(1) lookup
  int kingSq[2];   // Cached king squares for WHITE/BLACK

  Board();
  void ParseFen(const char *fen);
  void Print();

  // Helpers
  static int GetSquare(const char *sq);
  static void PrintBitboard(U64 bb);
  void UpdateOccupancies();
  U64 GenerateZobristHash();
};

// Bitboard Macros
#define POP_BIT(bb, sq) ((bb) &= ~(1ULL << (sq)))
#define SET_BIT(bb, sq) ((bb) |= (1ULL << (sq)))
#define GET_BIT(bb, sq) ((bb) & (1ULL << (sq)))

// Attack Tables
extern U64 pawn_attacks[2][64];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];

// Hash Keys
extern U64 piece_keys[13][64];
extern U64 side_key;
extern U64 castle_keys[16];

// Eval Masks
extern U64 FileMasks[8];
extern U64 RankMasks[8];
extern U64 BlackPassedMask[64];
extern U64 WhitePassedMask[64];
extern U64 IsolatedMask[64];

// TT flags
enum { TT_NONE, TT_ALPHA, TT_BETA, TT_EXACT };

struct TTEntry {
  std::atomic<U64> key;
  std::atomic<U64> data;
};

#define TT_SIZE 0x400000 // 4M entries (~100MB)

extern TTEntry
    *TranspositionTable; // Dynamic allocation better? Or static array?
// Static array in implementation. here extern pointer or array?
// Just `extern TTEntry TranspositionTable[TT_SIZE];` if generic.
// But stack/data segment limits? 100MB data segment might be fine.
// Let's use dynamic.

void InitHash();
void InitTT();
void ClearTT();
int ProbeTT(U64 key, int depth, int alpha, int beta, int &move);
void StoreTT(U64 key, int depth, int score, int flags, int move);

// Init functions
void InitAll();

// Move List
struct MoveList {
  Move moves[256];
  int count;
};

// Functions
void GenerateMoves(Board &board, MoveList &list, bool captures = false);
int MakeMove(Board &board, Move move,
             int capture); // Returns 0 if illegal (king in check)
void TakeBack(Board &board, Move move);
int IsSquareAttacked(int sq, int side, Board &board);

// Helpers to Pack/Unpack moves if needed, or just use struct
// For now struct is fine.

// Magic Bitboards
struct Magic {
  U64 *attacks;
  U64 mask;
  U64 magic;
  int shift;
};

extern Magic rookMagics[64];
extern Magic bishopMagics[64];

// Move Gen Helpers
inline U64 GetBishopAttacks(int sq, U64 occupancy) {
  unsigned int index = (unsigned int)(((occupancy & bishopMagics[sq].mask) *
                                       bishopMagics[sq].magic) >>
                                      bishopMagics[sq].shift);
  return bishopMagics[sq].attacks[index];
}

inline U64 GetRookAttacks(int sq, U64 occupancy) {
  unsigned int index = (unsigned int)(((occupancy & rookMagics[sq].mask) *
                                       rookMagics[sq].magic) >>
                                      rookMagics[sq].shift);
  return rookMagics[sq].attacks[index];
}

inline U64 GetQueenAttacks(int sq, U64 occupancy) {
  return GetBishopAttacks(sq, occupancy) | GetRookAttacks(sq, occupancy);
}

// Search
// Search Data (Thread Local)
#include <mutex>

#define MAX_PLY 64

struct SearchData {
  long long nodes;
  long long q_nodes;
  long long tt_hits;
  int killer_moves[2][MAX_PLY];
  int history_moves[13][64];
  int counter_moves[13][64]; // counter-move: indexed by [prev_piece][prev_to]

  SearchData() {
    nodes = 0;
    q_nodes = 0;
    tt_hits = 0;
    for (int i = 0; i < MAX_PLY; i++) {
      killer_moves[0][i] = 0;
      killer_moves[1][i] = 0;
    }
    for (int i = 0; i < 13; i++)
      for (int j = 0; j < 64; j++) {
        history_moves[i][j] = 0;
        counter_moves[i][j] = 0;
      }
  }
};

extern std::atomic<bool> stop_search;

// Perft remains global/simple for now
long Perft(Board &board, int depth);

// Search Functions with Thread Context
int AlphaBeta(Board &board, int depth, int alpha, int beta, int ply,
              bool do_null, SearchData &sd,
              int prev_piece = EMPTY, int prev_to = -1);
int Quiescence(Board &board, int alpha, int beta, SearchData &sd);
void PickNextMove(int moveNum, MoveList &list, Board &board, int tt_move,
                  int ply, SearchData &sd);
int ScoreMove(Board &board, Move m, int tt_move, int ply, SearchData &sd,
              int prev_piece = EMPTY, int prev_to = -1);
int SEE(Board &board, Move move);

// Main Entry Point (Spawns threads)
Move SearchPosition(Board &board, int depth);
extern std::atomic<long long> time_movegen;
extern std::atomic<long long> time_makemove;
extern std::atomic<long long> time_eval;
extern std::atomic<long long> time_attack;
extern std::atomic<long long> time_search;

int Evaluate(Board &board);

// UCI
void UciLoop();

#endif
