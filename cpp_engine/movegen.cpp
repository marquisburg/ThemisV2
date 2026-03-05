#include "defs.h"

std::atomic<long long> time_movegen(0);

// Helper to add move
void AddMove(MoveList &list, Move m) {
  list.moves[list.count] = m;
  list.count++;
}

// On-the-fly Slider Attacks (Removed - Replaced by Magic Bitboards in defs.h)

void GenerateMoves(Board &board, MoveList &list, bool captures) {
  list.count = 0;
  int side = board.side;
  int enemy = side ^ 1;
  U64 units = board.occupancies[side];
  U64 enemies = board.occupancies[enemy];
  U64 both = board.occupancies[BOTH];

  // Define Piece Types
  int P = (side == WHITE ? wP : bP);
  int N = (side == WHITE ? wN : bN);
  int B = (side == WHITE ? wB : bB);
  int R = (side == WHITE ? wR : bR);
  int Q = (side == WHITE ? wQ : bQ);
  int K = (side == WHITE ? wK : bK);

  // Pawns
  U64 bb = board.pieces[P];
  while (bb) {
    int sq = __builtin_ctzll(bb);
    POP_BIT(bb, sq);

    int to_sq;
    // Quiet Moves
    if (!captures) {
      if (side == WHITE) {
        to_sq = sq + 8;
        if (to_sq < 64 && !GET_BIT(both, to_sq)) {
          // Promotion
          if (to_sq >= A8) {
            AddMove(list, {sq, to_sq, P, 0, wQ});
            AddMove(list, {sq, to_sq, P, 0, wR});
            AddMove(list, {sq, to_sq, P, 0, wB});
            AddMove(list, {sq, to_sq, P, 0, wN});
          } else {
            AddMove(list, {sq, to_sq, P, 0, 0});
            // Double Push
            if ((sq >= A2 && sq <= H2) && !GET_BIT(both, (sq + 16))) {
              AddMove(list, {sq, sq + 16, P, 0, 0});
            }
          }
        }
      } else {
        // Black Quiet
        to_sq = sq - 8;
        if (to_sq >= 0 && !GET_BIT(both, to_sq)) {
          if (to_sq <= H1) {
            AddMove(list, {sq, to_sq, P, 0, bQ});
            AddMove(list, {sq, to_sq, P, 0, bR});
            AddMove(list, {sq, to_sq, P, 0, bB});
            AddMove(list, {sq, to_sq, P, 0, bN});
          } else {
            AddMove(list, {sq, to_sq, P, 0, 0});
            if ((sq >= A7 && sq <= H7) && !GET_BIT(both, (sq - 16))) {
              AddMove(list, {sq, sq - 16, P, 0, 0});
            }
          }
        }
      }
    }

    // Captures
    if (side == WHITE) {
      U64 attacks = pawn_attacks[WHITE][sq] & enemies;
      while (attacks) {
        to_sq = __builtin_ctzll(attacks);
        POP_BIT(attacks, to_sq);
        if (to_sq >= A8) {
          AddMove(list, {sq, to_sq, P, 1, wQ});
          AddMove(list, {sq, to_sq, P, 1, wR});
          AddMove(list, {sq, to_sq, P, 1, wB});
          AddMove(list, {sq, to_sq, P, 1, wN});
        } else {
          AddMove(list, {sq, to_sq, P, 1, 0});
        }
      }
      // En Passant
      if (board.enPas != NO_SQ) {
        U64 ep_attack = pawn_attacks[WHITE][sq] & (1ULL << board.enPas);
        if (ep_attack) {
          AddMove(list, {sq, board.enPas, P, 1, 0});
        }
      }
    } else {
      // Black Captures
      U64 attacks = pawn_attacks[BLACK][sq] & enemies;
      while (attacks) {
        to_sq = __builtin_ctzll(attacks);
        POP_BIT(attacks, to_sq);
        if (to_sq <= H1) {
          AddMove(list, {sq, to_sq, P, 1, bQ});
          AddMove(list, {sq, to_sq, P, 1, bR});
          AddMove(list, {sq, to_sq, P, 1, bB});
          AddMove(list, {sq, to_sq, P, 1, bN});
        } else {
          AddMove(list, {sq, to_sq, P, 1, 0});
        }
      }
      // En Passant
      if (board.enPas != NO_SQ) {
        U64 ep_attack = pawn_attacks[BLACK][sq] & (1ULL << board.enPas);
        if (ep_attack) {
          AddMove(list, {sq, board.enPas, P, 1, 0});
        }
      }
    }
  }

  // Knights
  bb = board.pieces[N];
  while (bb) {
    int sq = __builtin_ctzll(bb);
    POP_BIT(bb, sq);

    U64 attacks = knight_attacks[sq] & ~units;
    while (attacks) {
      int to = __builtin_ctzll(attacks);
      POP_BIT(attacks, to);
      int cap = GET_BIT(enemies, to) ? 1 : 0;
      if (captures && !cap)
        continue;
      AddMove(list, {sq, to, N, cap, 0});
    }
  }

  // Kings
  bb = board.pieces[K];
  if (bb) { // Should be 1
    int sq = __builtin_ctzll(bb);
    U64 attacks = king_attacks[sq] & ~units;
    while (attacks) {
      int to = __builtin_ctzll(attacks);
      POP_BIT(attacks, to);
      int cap = GET_BIT(enemies, to) ? 1 : 0;
      if (captures && !cap)
        continue;
      AddMove(list, {sq, to, K, cap, 0});
    }

    // Castling (quiet move generation only)
    if (!captures) {
      if (side == WHITE && sq == E1) {
        // White king-side: E1 -> G1 (rook H1 -> F1)
        if ((board.castle & WKCA) && board.squares[H1] == wR &&
            board.squares[F1] == EMPTY && board.squares[G1] == EMPTY &&
            !IsSquareAttacked(E1, BLACK, board) &&
            !IsSquareAttacked(F1, BLACK, board) &&
            !IsSquareAttacked(G1, BLACK, board)) {
          AddMove(list, {E1, G1, K, 0, 0});
        }

        // White queen-side: E1 -> C1 (rook A1 -> D1)
        if ((board.castle & WQCA) && board.squares[A1] == wR &&
            board.squares[D1] == EMPTY && board.squares[C1] == EMPTY &&
            board.squares[B1] == EMPTY &&
            !IsSquareAttacked(E1, BLACK, board) &&
            !IsSquareAttacked(D1, BLACK, board) &&
            !IsSquareAttacked(C1, BLACK, board)) {
          AddMove(list, {E1, C1, K, 0, 0});
        }
      } else if (side == BLACK && sq == E8) {
        // Black king-side: E8 -> G8 (rook H8 -> F8)
        if ((board.castle & BKCA) && board.squares[H8] == bR &&
            board.squares[F8] == EMPTY && board.squares[G8] == EMPTY &&
            !IsSquareAttacked(E8, WHITE, board) &&
            !IsSquareAttacked(F8, WHITE, board) &&
            !IsSquareAttacked(G8, WHITE, board)) {
          AddMove(list, {E8, G8, K, 0, 0});
        }

        // Black queen-side: E8 -> C8 (rook A8 -> D8)
        if ((board.castle & BQCA) && board.squares[A8] == bR &&
            board.squares[D8] == EMPTY && board.squares[C8] == EMPTY &&
            board.squares[B8] == EMPTY &&
            !IsSquareAttacked(E8, WHITE, board) &&
            !IsSquareAttacked(D8, WHITE, board) &&
            !IsSquareAttacked(C8, WHITE, board)) {
          AddMove(list, {E8, C8, K, 0, 0});
        }
      }
    }
  }

  // Sliders
  for (int p : {B, R, Q}) {
    bb = board.pieces[p];
    while (bb) {
      int sq = __builtin_ctzll(bb);
      POP_BIT(bb, sq);
      U64 attacks = 0;
      if (p == B)
        attacks = GetBishopAttacks(sq, both);
      else if (p == R)
        attacks = GetRookAttacks(sq, both);
      else
        attacks = GetQueenAttacks(sq, both);

      attacks &= ~units;
      while (attacks) {
        int to = __builtin_ctzll(attacks);
        POP_BIT(attacks, to);
        int cap = GET_BIT(enemies, to) ? 1 : 0;
        if (captures && !cap)
          continue;
        AddMove(list, {sq, to, p, cap, 0});
      }
    }
  }

}

void Board::PrintBitboard(U64 bb) {
  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      int sq = rank * 8 + file;
      if (GET_BIT(bb, sq))
        std::cout << "1 ";
      else
        std::cout << ". ";
    }
    std::cout << "\n";
  }
}
