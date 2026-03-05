#include "defs.h"
#include <algorithm>
#include <cstdlib>

std::atomic<long long> time_eval(0);

// Basic Material and PST Weights
const int mg_value[6] = {82, 337, 365, 477, 1025, 0};
const int eg_value[6] = {94, 281, 297, 512, 936, 0};

// Bonuses and Penalties
const int bishop_pair_bonus = 30;
const int rook_on_open_file_bonus = 20;
const int rook_on_semi_open_file_bonus = 10;
const int rook_on_seventh_bonus = 20;
const int pawn_isolated_penalty = -10;
const int pawn_doubled_penalty = -10;
const int mg_pawn_passed_bonus[8] = {0, 5, 10, 20, 35, 60, 100, 200};
const int eg_pawn_passed_bonus[8] = {0, 10, 20, 40, 70, 120, 200, 350};
const int king_shield_bonus = 5;
const int king_open_file_penalty = -15;

// King PSTs
const int mg_king_table[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50,
    -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40,
    -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30,
    -20, -10, -20, -20, -20, -20, -20, -20, -10, 20,  20,  0,   0,
    0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

const int eg_king_table[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,
    -10, -20, -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -10,
    30,  40,  40,  30,  -10, -30, -30, -10, 30,  40,  40,  30,  -10,
    -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -30, 0,   0,
    0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

const int pawn_pst[] = {0,   0,  0,  0,   0,  0,  0,   0,  50, 50, 50, 50, 50,
                        50,  50, 50, 10,  10, 20, 30,  30, 20, 10, 10, 5,  5,
                        10,  25, 25, 10,  5,  5,  0,   0,  0,  20, 20, 0,  0,
                        0,   5,  -5, -10, 0,  0,  -10, -5, 5,  5,  10, 10, -20,
                        -20, 10, 10, 5,   0,  0,  0,   0,  0,  0,  0,  0};

const int knight_pst[] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,
                          0,   0,   0,   -20, -40, -30, 0,   10,  15,  15,  10,
                          0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30, -30,
                          0,   15,  20,  20,  15,  0,   -30, -30, 5,   10,  15,
                          15,  10,  5,   -30, -40, -20, 0,   5,   5,   0,   -20,
                          -40, -50, -40, -30, -30, -30, -30, -40, -50};

const int bishop_pst[] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,
                          0,   0,   0,   0,   -10, -10, 0,   5,   10,  10,  5,
                          0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10, -10,
                          0,   10,  10,  10,  10,  0,   -10, -10, 10,  10,  10,
                          10,  10,  10,  -10, -10, 5,   0,   0,   0,   0,   5,
                          -10, -20, -10, -10, -10, -10, -10, -10, -20};

const int rook_pst[] = {0,  0,  0, 0,  0, 0,  0,  0, 5,  10, 10, 10, 10,
                        10, 10, 5, -5, 0, 0,  0,  0, 0,  0,  -5, -5, 0,
                        0,  0,  0, 0,  0, -5, -5, 0, 0,  0,  0,  0,  0,
                        -5, -5, 0, 0,  0, 0,  0,  0, -5, -5, 0,  0,  0,
                        0,  0,  0, -5, 0, 0,  0,  5, 5,  0,  0,  0};

int Evaluate(Board &board) {
  // auto start = std::chrono::high_resolution_clock::now(); // REMOVED

  int mg_score[2] = {0, 0};
  int eg_score[2] = {0, 0};

  // 1. Logic to count pieces using __builtin_popcountll
  U64 wp = board.pieces[wP], bp = board.pieces[bP];
  U64 wn = board.pieces[wN], bn = board.pieces[bN];
  U64 wb = board.pieces[wB], bb = board.pieces[bB];
  U64 wr = board.pieces[wR], br = board.pieces[bR];
  U64 wq = board.pieces[wQ], bq = board.pieces[bQ];
  U64 wk = board.pieces[wK], bk = board.pieces[bK];

  int wksq = board.kingSq[WHITE];
  int bksq = board.kingSq[BLACK];
  if (wksq < 0)
    wksq = E1;
  if (bksq < 0)
    bksq = E8;

  int wp_cnt = __builtin_popcountll(wp), bp_cnt = __builtin_popcountll(bp);
  int wn_cnt = __builtin_popcountll(wn), bn_cnt = __builtin_popcountll(bn);
  int wb_cnt = __builtin_popcountll(wb), bb_cnt = __builtin_popcountll(bb);
  int wr_cnt = __builtin_popcountll(wr), br_cnt = __builtin_popcountll(br);
  int wq_cnt = __builtin_popcountll(wq), bq_cnt = __builtin_popcountll(bq);

  // 2. Game Phase Calculation
  // Total Phase = 24.
  // Knights/Bishops = 1, Rooks = 2, Queens = 4.
  int phase = wn_cnt + bn_cnt + wb_cnt + bb_cnt + wr_cnt * 2 + br_cnt * 2 +
              wq_cnt * 4 + bq_cnt * 4;
  if (phase > 24)
    phase = 24;

  // 3. Material Score
  mg_score[WHITE] = wp_cnt * mg_value[0] + wn_cnt * mg_value[1] +
                    wb_cnt * mg_value[2] + wr_cnt * mg_value[3] +
                    wq_cnt * mg_value[4];
  eg_score[WHITE] = wp_cnt * eg_value[0] + wn_cnt * eg_value[1] +
                    wb_cnt * eg_value[2] + wr_cnt * eg_value[3] +
                    wq_cnt * eg_value[4];

  mg_score[BLACK] = bp_cnt * mg_value[0] + bn_cnt * mg_value[1] +
                    bb_cnt * mg_value[2] + br_cnt * mg_value[3] +
                    bq_cnt * mg_value[4];
  eg_score[BLACK] = bp_cnt * eg_value[0] + bn_cnt * eg_value[1] +
                    bb_cnt * eg_value[2] + br_cnt * eg_value[3] +
                    bq_cnt * eg_value[4];

  // 4. Positional Scoring Loops
  U64 bitboard;
  int sq;

  // --- King Safety Counters ---
  int white_safety_attackers = 0;
  int white_safety_units = 0;
  int black_safety_attackers = 0;
  int black_safety_units = 0;

  U64 w_king_ring = king_attacks[wksq] | (1ULL << wksq);
  U64 b_king_ring = king_attacks[bksq] | (1ULL << bksq);

  // Optimized Pawn Attack Calculation
  U64 white_pawn_attacks =
      ((wp << 7) & 0x7f7f7f7f7f7f7f7fULL) | ((wp << 9) & 0xfefefefefefefefeULL);
  U64 black_pawn_attacks =
      ((bp >> 7) & 0xfefefefefefefefeULL) | ((bp >> 9) & 0x7f7f7f7f7f7f7f7fULL);

  // Development / Tempo (Opening only)
  if (phase > 20) {
    // White development
    int white_minor_dev = 0;
    if (!(wn & (1ULL << B1)))
      white_minor_dev++;
    if (!(wn & (1ULL << G1)))
      white_minor_dev++;
    if (!(wb & (1ULL << C1)))
      white_minor_dev++;
    if (!(wb & (1ULL << F1)))
      white_minor_dev++;

    // Penalty for minor pieces on back rank
    if (wn & (1ULL << B1))
      mg_score[WHITE] -= 15;
    if (wn & (1ULL << G1))
      mg_score[WHITE] -= 15;
    if (wb & (1ULL << C1))
      mg_score[WHITE] -= 15;
    if (wb & (1ULL << F1))
      mg_score[WHITE] -= 15;

    // Early Queen move
    if (!(wq & (1ULL << D1)) && white_minor_dev <= 2)
      mg_score[WHITE] -= 25;

    // Knights blocking C-pawn (bad for development in some openings)
    if (GET_BIT(wn, C3) && GET_BIT(wp, C2))
      mg_score[WHITE] -= 10;

    // Castling and King Safety in Opening
    bool white_castled = (wksq == G1 || wksq == C1);
    if (white_castled) {
      mg_score[WHITE] += 40;
    } else {
      // Penalty for losing castling rights without castling
      if (!(board.castle & (WKCA | WQCA))) {
        mg_score[WHITE] -= 50;
      }
      // Penalty for king staying in center too long
      if (wksq == E1) {
        mg_score[WHITE] -= 20;
      }
    }

    // Black development
    int black_minor_dev = 0;
    if (!(bn & (1ULL << B8)))
      black_minor_dev++;
    if (!(bn & (1ULL << G8)))
      black_minor_dev++;
    if (!(bb & (1ULL << C8)))
      black_minor_dev++;
    if (!(bb & (1ULL << F8)))
      black_minor_dev++;

    if (bn & (1ULL << B8))
      mg_score[BLACK] -= 15;
    if (bn & (1ULL << G8))
      mg_score[BLACK] -= 15;
    if (bb & (1ULL << C8))
      mg_score[BLACK] -= 15;
    if (bb & (1ULL << F8))
      mg_score[BLACK] -= 15;

    if (!(bq & (1ULL << D8)) && black_minor_dev <= 2)
      mg_score[BLACK] -= 25;

    if (GET_BIT(bn, C6) && GET_BIT(bp, C7))
      mg_score[BLACK] -= 10;

    bool black_castled = (bksq == G8 || bksq == C8);
    if (black_castled) {
      mg_score[BLACK] += 40;
    } else {
      if (!(board.castle & (BKCA | BQCA))) {
        mg_score[BLACK] -= 50;
      }
      if (bksq == E8) {
        mg_score[BLACK] -= 20;
      }
    }
  }

  // --- White Pawns ---
  bitboard = wp;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    mg_score[WHITE] += pawn_pst[sq];
    eg_score[WHITE] += pawn_pst[sq];

    int file = sq & 7;

    // Doubled Pawns
    if (__builtin_popcountll(wp & FileMasks[file]) > 1) {
      mg_score[WHITE] += pawn_doubled_penalty;
      eg_score[WHITE] += pawn_doubled_penalty;
    }
    // Isolated Pawns
    if ((wp & IsolatedMask[file]) == 0) {
      mg_score[WHITE] += pawn_isolated_penalty;
      eg_score[WHITE] += pawn_isolated_penalty;
    }
    // Passed Pawns
    if ((WhitePassedMask[sq] & bp) == 0) {
      int rank = sq >> 3; // 0-7
      mg_score[WHITE] += mg_pawn_passed_bonus[rank];
      eg_score[WHITE] += eg_pawn_passed_bonus[rank];

      // King proximity bonus in endgame: reward own king close, penalize enemy king close.
      if (rank >= 3) {
        int own_dist = std::max(std::abs((wksq >> 3) - rank), std::abs((wksq & 7) - (sq & 7)));
        int opp_dist = std::max(std::abs((bksq >> 3) - rank), std::abs((bksq & 7) - (sq & 7)));
        eg_score[WHITE] += (opp_dist - own_dist) * 5 * (rank - 2);
      }
    }
  }

  // --- Black Pawns ---
  bitboard = bp;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    int msq = sq ^ 56; // Flip square for PST
    mg_score[BLACK] += pawn_pst[msq];
    eg_score[BLACK] += pawn_pst[msq];

    int file = sq & 7;

    if (__builtin_popcountll(bp & FileMasks[file]) > 1) {
      mg_score[BLACK] += pawn_doubled_penalty;
      eg_score[BLACK] += pawn_doubled_penalty;
    }
    if ((bp & IsolatedMask[file]) == 0) {
      mg_score[BLACK] += pawn_isolated_penalty;
      eg_score[BLACK] += pawn_isolated_penalty;
    }
    if ((BlackPassedMask[sq] & wp) == 0) {
      int rank = 7 - (sq >> 3); // optimize rank calc
      mg_score[BLACK] += mg_pawn_passed_bonus[rank];
      eg_score[BLACK] += eg_pawn_passed_bonus[rank];

      // King proximity bonus in endgame.
      if (rank >= 3) {
        int own_dist = std::max(std::abs((bksq >> 3) - (sq >> 3)), std::abs((bksq & 7) - (sq & 7)));
        int opp_dist = std::max(std::abs((wksq >> 3) - (sq >> 3)), std::abs((wksq & 7) - (sq & 7)));
        eg_score[BLACK] += (opp_dist - own_dist) * 5 * (rank - 2);
      }
    }
  }

  // --- White Knights ---
  bitboard = wn;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    mg_score[WHITE] += knight_pst[sq];
    eg_score[WHITE] += knight_pst[sq];

    U64 attacks = knight_attacks[sq];

    U64 safe_moves = attacks & ~black_pawn_attacks;
    int mobility = __builtin_popcountll(safe_moves);
    mg_score[WHITE] += (mobility - 3) * 2;
    eg_score[WHITE] += (mobility - 3) * 2;

    // King safety tracking
    if (attacks & b_king_ring) {
      white_safety_attackers++;
      white_safety_units += 2;
    }

    // Outpost (Protected by pawn)
    if (pawn_attacks[BLACK][sq] & wp) {
      int rank = sq >> 3;
      if (rank >= 2 && rank <= 5) {
        mg_score[WHITE] += 20;
        eg_score[WHITE] += 10;
      }
    }

    // Tempo: Attacking Major Pieces
    if (attacks & bq)
      mg_score[WHITE] += 15;
    else if (attacks & br)
      mg_score[WHITE] += 10;

  }

  // --- Black Knights ---
  bitboard = bn;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    int msq = sq ^ 56;
    mg_score[BLACK] += knight_pst[msq];
    eg_score[BLACK] += knight_pst[msq];

    U64 attacks = knight_attacks[sq];

    U64 safe_moves = attacks & ~white_pawn_attacks;
    int mobility = __builtin_popcountll(safe_moves);
    mg_score[BLACK] += (mobility - 3) * 2;
    eg_score[BLACK] += (mobility - 3) * 2;

    if (attacks & w_king_ring) {
      black_safety_attackers++;
      black_safety_units += 2;
    }

    // Outpost
    if (pawn_attacks[WHITE][sq] & bp) {
      int rank = sq >> 3; // 0-7
      if (rank >= 2 && rank <= 5) {
        mg_score[BLACK] += 20;
        eg_score[BLACK] += 10;
      }
    }

    if (attacks & wq)
      mg_score[BLACK] += 15;
    else if (attacks & wr)
      mg_score[BLACK] += 10;

  }

  // --- White Bishops ---
  bitboard = wb;
  if (wb_cnt >= 2) {
    mg_score[WHITE] += bishop_pair_bonus;
    eg_score[WHITE] += bishop_pair_bonus;
  }
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    mg_score[WHITE] += bishop_pst[sq];
    eg_score[WHITE] += bishop_pst[sq];

    U64 attacks = GetBishopAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~black_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);

    mg_score[WHITE] += (mobility - 4) * 4;
    eg_score[WHITE] += (mobility - 4) * 4;

    if (attacks & b_king_ring) {
      white_safety_attackers++;
      white_safety_units += 2;
    }

    if (attacks & bq)
      mg_score[WHITE] += 15;
    else if (attacks & br)
      mg_score[WHITE] += 10;

  }

  // --- Black Bishops ---
  bitboard = bb;
  if (bb_cnt >= 2) {
    mg_score[BLACK] += bishop_pair_bonus;
    eg_score[BLACK] += bishop_pair_bonus;
  }
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    int msq = sq ^ 56;
    mg_score[BLACK] += bishop_pst[msq];
    eg_score[BLACK] += bishop_pst[msq];

    U64 attacks = GetBishopAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~white_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);

    mg_score[BLACK] += (mobility - 4) * 4;
    eg_score[BLACK] += (mobility - 4) * 4;

    if (attacks & w_king_ring) {
      black_safety_attackers++;
      black_safety_units += 2;
    }

    if (attacks & wq)
      mg_score[BLACK] += 15;
    else if (attacks & wr)
      mg_score[BLACK] += 10;

  }

  // --- White Rooks ---
  bitboard = wr;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    mg_score[WHITE] += rook_pst[sq];
    eg_score[WHITE] += rook_pst[sq];

    int file = sq & 7;

    U64 attacks = GetRookAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~black_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);
    mg_score[WHITE] += (mobility - 4) * 2;
    eg_score[WHITE] += (mobility - 4) * 4;

    // King Safety tracking
    if (attacks & b_king_ring) {
      white_safety_attackers++;
      white_safety_units += 3;
    }

    // Open File Detection
    if ((FileMasks[file] & wp) == 0) {
      if ((FileMasks[file] & bp) == 0) {
        // Fully Open
        mg_score[WHITE] += rook_on_open_file_bonus;
        eg_score[WHITE] += rook_on_open_file_bonus;
      } else {
        // Semi Open
        mg_score[WHITE] += rook_on_semi_open_file_bonus;
        eg_score[WHITE] += rook_on_semi_open_file_bonus;
      }
    }

    // Rook on 7th rank
    if (sq >= 48 && sq <= 55) {
      // 48 = 6 * 8. Rank 6 (0-indexed). Wait.
      // Rank 1: 0-7
      // ...
      // Rank 7: 48-55. Correct. This is standard terminology (Rank 7 is index
      // 6? No rank 1 is index 0). If sq >= 48, rank is 6 or 7? 48/8 = 6. Rooks
      // on SEVENTH rank. In chess "7th rank" for White is rank index 6. E.g.
      // A2-A7. The old code had: sq >= 48 && sq <= 55. This is Rank 7 (index 6,
      // white's 7th). But let's check enemy king rank logic. int
      // enemy_king_rank = (__builtin_ctzll(bk)) / 8; if (enemy_king_rank == 7)
      // ... Wait, if White rook is on Rank 7, and Black King is on Rank 8
      // (index 7). That makes sense. I will keep the constant literals 48, 55.

      int enemy_king_rank = bksq >> 3;
      if (enemy_king_rank == 7) {
        mg_score[WHITE] += rook_on_seventh_bonus;
        eg_score[WHITE] += rook_on_seventh_bonus;
      }
    }

    // Connected Rooks
    if (board.pieces[wR] & GetRookAttacks(sq, board.occupancies[BOTH])) {
      mg_score[WHITE] += 10;
    }

  }

  // --- Black Rooks ---
  bitboard = br;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    int msq = sq ^ 56;
    mg_score[BLACK] += rook_pst[msq];
    eg_score[BLACK] += rook_pst[msq];

    int file = sq & 7;

    U64 attacks = GetRookAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~white_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);

    mg_score[BLACK] += (mobility - 4) * 2;
    eg_score[BLACK] += (mobility - 4) * 2;

    if ((FileMasks[file] & bp) == 0) {
      if ((FileMasks[file] & wp) == 0) {
        mg_score[BLACK] += rook_on_open_file_bonus;
        eg_score[BLACK] += rook_on_open_file_bonus;
      } else {
        mg_score[BLACK] += rook_on_semi_open_file_bonus;
        eg_score[BLACK] += rook_on_semi_open_file_bonus;
      }
    }

    // Rook on 2nd rank (Black's 7th). Ranks 0-7.
    // Rank 2 is index 1.
    // sq >= 8 && sq <= 15. Correct.
    if (sq >= 8 && sq <= 15) {
      int enemy_king_rank = wksq >> 3;
      if (enemy_king_rank == 0) {
        mg_score[BLACK] += rook_on_seventh_bonus;
        eg_score[BLACK] += rook_on_seventh_bonus;
      }
    }

    // Connected Rooks
    if (board.pieces[bR] & GetRookAttacks(sq, board.occupancies[BOTH])) {
      mg_score[BLACK] += 10;
    }

    if (attacks & w_king_ring) {
      black_safety_attackers++;
      black_safety_units += 3;
    }

  }

  // --- Queens ---
  bitboard = wq;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    U64 attacks = GetQueenAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~black_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);
    mg_score[WHITE] += (mobility - 9) * 1;
    eg_score[WHITE] += (mobility - 9) * 2;

    if (attacks & b_king_ring) {
      white_safety_attackers++;
      white_safety_units += 5;
    }
  }
  bitboard = bq;
  while (bitboard) {
    sq = __builtin_ctzll(bitboard);
    POP_BIT(bitboard, sq);
    U64 attacks = GetQueenAttacks(sq, board.occupancies[BOTH]);

    U64 safe_attacks = attacks & ~white_pawn_attacks;
    int mobility = __builtin_popcountll(safe_attacks);
    mg_score[BLACK] += (mobility - 9) * 1;
    eg_score[BLACK] += (mobility - 9) * 2;

    if (attacks & w_king_ring) {
      black_safety_attackers++;
      black_safety_units += 5;
    }
  }

  // --- King Safety Evaluation ---
  if (phase > 8) {
    // White King Safety
    mg_score[WHITE] += mg_king_table[wksq];
    eg_score[WHITE] += eg_king_table[wksq];

    if (phase > 12) {
      // Pawn Shield
      int r = wksq >> 3;
      int f = wksq & 7;
      if (r < 7) {
        U64 shield_mask = 0;
        if (f > 0)
          SET_BIT(shield_mask, wksq + 7);
        SET_BIT(shield_mask, wksq + 8);
        if (f < 7)
          SET_BIT(shield_mask, wksq + 9);
        int shield_pawns = __builtin_popcountll(shield_mask & wp);
        mg_score[WHITE] += shield_pawns * king_shield_bonus;

        if ((FileMasks[f] & wp) == 0)
          mg_score[WHITE] += king_open_file_penalty;
      }

      // Attack Penalty
      if (black_safety_attackers >= 2) {
        int penalty = (black_safety_units * black_safety_units) / 2;
        mg_score[WHITE] -= penalty;
      }
    }

    // Black King Safety
    mg_score[BLACK] += mg_king_table[bksq ^ 56];
    eg_score[BLACK] += eg_king_table[bksq ^ 56];

    if (phase > 12) {
      int r = bksq >> 3;
      int f = bksq & 7;
      if (r > 0) {
        U64 shield_mask = 0;
        if (f > 0)
          SET_BIT(shield_mask, bksq - 9);
        SET_BIT(shield_mask, bksq - 8);
        if (f < 7)
          SET_BIT(shield_mask, bksq - 7);
        int shield_pawns = __builtin_popcountll(shield_mask & bp);
        mg_score[BLACK] += shield_pawns * king_shield_bonus;

        if ((FileMasks[f] & bp) == 0)
          mg_score[BLACK] += king_open_file_penalty;
      }

      if (white_safety_attackers >= 2) {
        int penalty = (white_safety_units * white_safety_units) / 2;
        mg_score[BLACK] -= penalty;
      }
    }
  }

  // Tapered Eval
  int mg = mg_score[WHITE] - mg_score[BLACK];
  int eg = eg_score[WHITE] - eg_score[BLACK];

  int final_score = ((mg * phase) + (eg * (24 - phase))) / 24;

  // Tempo bonus (side to move)
  final_score += 10;

  // auto end = std::chrono::high_resolution_clock::now(); // REMOVED
  // time_eval +=
  //    std::chrono::duration_cast<std::chrono::nanoseconds>(end -
  //    start).count(); // REMOVED

  if (board.side == WHITE)
    return final_score;
  return -final_score;
}
