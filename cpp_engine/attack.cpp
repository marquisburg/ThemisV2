#include "defs.h"

std::atomic<long long> time_attack(0);

int IsSquareAttacked(int sq, int attacker_side, Board &board) {
  U64 occupancy = board.occupancies[BOTH];
  if (attacker_side == WHITE) {
    U64 white_bq = board.pieces[wB] | board.pieces[wQ];
    U64 white_rq = board.pieces[wR] | board.pieces[wQ];
    if (pawn_attacks[BLACK][sq] & board.pieces[wP])
      return 1;
    if (knight_attacks[sq] & board.pieces[wN])
      return 1;
    if (white_bq && (GetBishopAttacks(sq, occupancy) & white_bq))
      return 1;
    if (white_rq && (GetRookAttacks(sq, occupancy) & white_rq))
      return 1;
    if (king_attacks[sq] & board.pieces[wK])
      return 1;
  } else {
    U64 black_bq = board.pieces[bB] | board.pieces[bQ];
    U64 black_rq = board.pieces[bR] | board.pieces[bQ];
    if (pawn_attacks[WHITE][sq] & board.pieces[bP])
      return 1;
    if (knight_attacks[sq] & board.pieces[bN])
      return 1;
    if (black_bq && (GetBishopAttacks(sq, occupancy) & black_bq))
      return 1;
    if (black_rq && (GetRookAttacks(sq, occupancy) & black_rq))
      return 1;
    if (king_attacks[sq] & board.pieces[bK])
      return 1;
  }

  return 0;
}

// SEE Values
const int see_values[13] = {100, 300, 300, 500, 900,   10000, 100,
                            300, 300, 500, 900, 10000, 0};

int GetSmallestAttacker(Board &board, int sq, int side_to_move,
                        int &attacker_piece) {
  // Pawn
  int pawn = (side_to_move == WHITE) ? wP : bP;
  U64 pawns = board.pieces[pawn];
  U64 pawn_att = pawn_attacks[side_to_move ^ 1][sq] & pawns;
  if (pawn_att) {
    attacker_piece = pawn;
    return __builtin_ctzll(pawn_att);
  }

  // Knight
  int knight = (side_to_move == WHITE) ? wN : bN;
  U64 knights = board.pieces[knight];
  U64 knight_att = knight_attacks[sq] & knights;
  if (knight_att) {
    attacker_piece = knight;
    return __builtin_ctzll(knight_att);
  }

  // Bishop
  int bishop = (side_to_move == WHITE) ? wB : bB;
  U64 bishops = board.pieces[bishop];
  U64 bishop_att = GetBishopAttacks(sq, board.occupancies[BOTH]) & bishops;
  if (bishop_att) {
    attacker_piece = bishop;
    return __builtin_ctzll(bishop_att);
  }

  // Rook
  int rook = (side_to_move == WHITE) ? wR : bR;
  U64 rooks = board.pieces[rook];
  U64 rook_att = GetRookAttacks(sq, board.occupancies[BOTH]) & rooks;
  if (rook_att) {
    attacker_piece = rook;
    return __builtin_ctzll(rook_att);
  }

  // Queen
  int queen = (side_to_move == WHITE) ? wQ : bQ;
  U64 queens = board.pieces[queen];
  U64 queen_att = GetQueenAttacks(sq, board.occupancies[BOTH]) & queens;
  if (queen_att) {
    attacker_piece = queen;
    return __builtin_ctzll(queen_att);
  }

  // King
  int king = (side_to_move == WHITE) ? wK : bK;
  U64 kings = board.pieces[king];
  U64 king_att = king_attacks[sq] & kings;
  if (king_att) {
    attacker_piece = king;
    return __builtin_ctzll(king_att);
  }

  attacker_piece = EMPTY;
  return -1;
}

int SEE(Board &board, Move move) {
  if (!move.capture)
    return 0;

  int gain[32];
  int d = 0;

  int from = move.from;
  int to = move.to;
  int piece = move.piece;
  int captured = board.squares[to];
  if (captured == EMPTY && to == board.enPas && (piece == wP || piece == bP)) {
    captured = (piece == wP) ? bP : wP;
  }
  if (captured == EMPTY)
    return 0;

  gain[d] = see_values[captured];

  int attacker = piece;
  int side = board.side;

  U64 occupancy = board.occupancies[BOTH];
  POP_BIT(occupancy, from); // Make the first move

  // Loop
  while (true) {
    d++;
    // Swap side
    side ^= 1;
    gain[d] = see_values[attacker] - gain[d - 1];

    if (std::max(-gain[d - 1], gain[d]) < 0)
      break;

    // Find next attacker
    // Uses occupancy that we modify (X-Ray support)

    // RE-IMPLEMENT Smallest Attacker logic with modified occupancy
    // Need to pass occupancy to GetAttacks

    // Attacker finding with X-Ray support requires specialized function or just
    // inline here. Let's inline simplified version or update SmallestAttacker.

    int next_attacker_sq = -1;
    int next_attacker_piece = EMPTY;

    // Pawns
    int p_type = (side == WHITE) ? wP : bP;
    if (pawn_attacks[side ^ 1][to] & board.pieces[p_type] & occupancy) {
      next_attacker_piece = p_type;
      // Find LSB of intersection
      U64 atts = pawn_attacks[side ^ 1][to] & board.pieces[p_type] & occupancy;
      next_attacker_sq = __builtin_ctzll(atts);
    } else {
      // Knights
      int n_type = (side == WHITE) ? wN : bN;
      if (knight_attacks[to] & board.pieces[n_type] & occupancy) {
        next_attacker_piece = n_type;
        U64 atts = knight_attacks[to] & board.pieces[n_type] & occupancy;
        next_attacker_sq = __builtin_ctzll(atts);
      } else {
        // Bishops
        int b_type = (side == WHITE) ? wB : bB;
        U64 b_att =
            GetBishopAttacks(to, occupancy) & board.pieces[b_type] & occupancy;
        if (b_att) {
          next_attacker_piece = b_type;
          next_attacker_sq = __builtin_ctzll(b_att);
        } else {
          // Rooks
          int r_type = (side == WHITE) ? wR : bR;
          U64 r_att =
              GetRookAttacks(to, occupancy) & board.pieces[r_type] & occupancy;
          if (r_att) {
            next_attacker_piece = r_type;
            next_attacker_sq = __builtin_ctzll(r_att);
          } else {
            // Queens
            int q_type = (side == WHITE) ? wQ : bQ;
            U64 q_att = GetQueenAttacks(to, occupancy) & board.pieces[q_type] &
                        occupancy;
            if (q_att) {
              next_attacker_piece = q_type;
              next_attacker_sq = __builtin_ctzll(q_att);
            } else {
              // Kings
              int k_type = (side == WHITE) ? wK : bK;
              if (king_attacks[to] & board.pieces[k_type] & occupancy) {
                next_attacker_piece = k_type;
                next_attacker_sq = __builtin_ctzll(
                    king_attacks[to] & board.pieces[k_type] & occupancy);
              }
            }
          }
        }
      }
    }

    if (next_attacker_sq == -1)
      break;

    attacker = next_attacker_piece;
    POP_BIT(occupancy, next_attacker_sq);
  }

  while (d > 1) {
    d--;
    gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
  }

  return gain[0];
}
