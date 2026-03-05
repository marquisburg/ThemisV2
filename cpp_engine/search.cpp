#include "defs.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<bool> stop_search(false);
std::atomic<long long> time_search(0);

// MVV-LVA
const int victim_score[12] = {100, 200, 300, 400, 500, 600,
                              100, 200, 300, 400, 500, 600};

const int q_delta_values[13] = {100, 300, 300, 500, 900, 0,
                                100, 300, 300, 500, 900, 0, 0};

inline bool ShouldStopSearch() {
  return stop_search.load(std::memory_order_relaxed);
}

inline int PackMove(const Move &m) { return m.from | (m.to << 6); }

inline bool IsCastleMove(const Move &m) {
  return ((m.piece == wK || m.piece == bK) &&
          (m.to == m.from + 2 || m.to == m.from - 2));
}

inline int ClampPly(int ply) { return (ply < MAX_PLY) ? ply : (MAX_PLY - 1); }

inline int SideKingSquare(const Board &board, int side) {
  return board.kingSq[side];
}

int ScoreMove(Board &board, Move m, int tt_move, int ply, SearchData &sd,
              int prev_piece, int prev_to) {
  int packed = PackMove(m);
  if (packed == tt_move)
    return 30000;

  if (m.capture) {
    int victim = board.squares[m.to];
    if (victim == EMPTY && (m.piece == wP || m.piece == bP) &&
        m.to == board.enPas) {
      victim = (board.side == WHITE) ? bP : wP;
    }

    if (victim != EMPTY) {
      return 10000 + victim_score[victim] - (victim_score[m.piece] / 10);
    }
    return 9500;
  }

  // Promotion bonus.
  if (m.promoted)
    return 9800;

  int ply_index = ClampPly(ply);
  if (sd.killer_moves[0][ply_index] == packed)
    return 9000;
  if (IsCastleMove(m))
    return 8500;
  if (sd.killer_moves[1][ply_index] == packed)
    return 8000;

  // Counter-move heuristic.
  if (prev_piece != EMPTY && prev_to >= 0 && prev_to < 64) {
    if (sd.counter_moves[prev_piece][prev_to] == packed)
      return 7500;
  }

  return sd.history_moves[m.piece][m.to];
}

void PickNextMove(int moveNum, MoveList &list, Board &board, int tt_move,
                  int ply, SearchData &sd) {
  int bestScore = -1;
  int bestNum = moveNum;
  for (int i = moveNum; i < list.count; i++) {
    int score = ScoreMove(board, list.moves[i], tt_move, ply, sd);
    if (score > bestScore) {
      bestScore = score;
      bestNum = i;
    }
  }
  Move temp = list.moves[moveNum];
  list.moves[moveNum] = list.moves[bestNum];
  list.moves[bestNum] = temp;
}

void SortMoves(MoveList &list, Board &board, int tt_move, int ply,
               SearchData &sd, int prev_piece = EMPTY, int prev_to = -1) {
  int scores[256];
  for (int i = 0; i < list.count; i++) {
    scores[i] = ScoreMove(board, list.moves[i], tt_move, ply, sd, prev_piece, prev_to);
  }

  // Insertion sort is fast for typical small chess move lists.
  for (int i = 1; i < list.count; i++) {
    Move move_key = list.moves[i];
    int score_key = scores[i];
    int j = i - 1;

    while (j >= 0 && scores[j] < score_key) {
      list.moves[j + 1] = list.moves[j];
      scores[j + 1] = scores[j];
      j--;
    }

    list.moves[j + 1] = move_key;
    scores[j + 1] = score_key;
  }
}

int Quiescence(Board &board, int alpha, int beta, SearchData &sd) {
  sd.q_nodes++;
  if (ShouldStopSearch())
    return 0;

  int stand_pat = Evaluate(board);
  if (stand_pat >= beta)
    return beta;
  if (stand_pat > alpha)
    alpha = stand_pat;

  MoveList list;
  GenerateMoves(board, list, true);
  SortMoves(list, board, 0, 0, sd);

  for (int i = 0; i < list.count; i++) {
    Move move = list.moves[i];

    if (move.capture) {
      int victim = board.squares[move.to];
      if (victim == EMPTY && (move.piece == wP || move.piece == bP) &&
          move.to == board.enPas) {
        victim = (board.side == WHITE) ? bP : wP;
      }

      // Delta pruning: skip captures that cannot lift alpha even optimistically.
      if (!move.promoted) {
        int gain = (victim != EMPTY) ? q_delta_values[victim] : 100;
        if (stand_pat + gain + 200 <= alpha)
          continue;
      }

      // SEE pruning for losing captures.
      int attacker_val = q_delta_values[move.piece];
      int victim_val = (victim != EMPTY) ? q_delta_values[victim] : 100;
      if (victim_val + 50 < attacker_val) {
        if (SEE(board, move) < 0)
          continue;
      }
    }

    Board copy = board;
    if (!MakeMove(copy, move, move.capture))
      continue;

    int val = -Quiescence(copy, -beta, -alpha, sd);
    if (ShouldStopSearch())
      return 0;

    if (val >= beta)
      return beta;
    if (val > alpha)
      alpha = val;
  }

  return alpha;
}

int AlphaBeta(Board &board, int depth, int alpha, int beta, int ply,
              bool do_null, SearchData &sd,
              int prev_piece, int prev_to) {
  if (ShouldStopSearch())
    return 0;
  if (ply >= MAX_PLY - 1)
    return Evaluate(board);
  if (depth <= 0)
    return Quiescence(board, alpha, beta, sd);

  // Mate-distance pruning.
  const int mate_score = 49000;
  if (alpha < -mate_score + ply)
    alpha = -mate_score + ply;
  if (beta > mate_score - ply - 1)
    beta = mate_score - ply - 1;
  if (alpha >= beta)
    return alpha;

  sd.nodes++;

  int tt_move = 0;
  int tt_score = ProbeTT(board.zobristKey, depth, alpha, beta, tt_move);
  if (tt_score != -50001) {
    sd.tt_hits++;
    if (ply > 0)
      return tt_score;
  }

  int king_sq = SideKingSquare(board, board.side);
  bool in_check = (king_sq != -1) && IsSquareAttacked(king_sq, board.side ^ 1, board);
  int static_eval = -50001;

  // Reverse Futility Pruning (static null move pruning).
  if (depth <= 3 && ply > 0 && !in_check && std::abs(beta) < 9000) {
    static_eval = Evaluate(board);
    int margin = 120 * depth;
    if (static_eval - margin >= beta)
      return beta;
  }

  // Null Move Pruning — skip in pawn-only positions (zugzwang risk).
  if (do_null && depth >= 3 && ply > 0 && !in_check && std::abs(beta) < 9000) {
    int side = board.side;
    bool has_pieces = (side == WHITE)
        ? (board.pieces[wN] | board.pieces[wB] | board.pieces[wR] | board.pieces[wQ]) != 0
        : (board.pieces[bN] | board.pieces[bB] | board.pieces[bR] | board.pieces[bQ]) != 0;

    if (has_pieces) {
      Board copy = board;
      copy.side ^= 1;
      copy.zobristKey ^= side_key;
      if (copy.enPas != NO_SQ) {
        copy.zobristKey ^= piece_keys[12][copy.enPas];
        copy.enPas = NO_SQ;
      }

      int R = 2 + (depth >= 8 ? 1 : 0);
      int val = -AlphaBeta(copy, depth - 1 - R, -beta, -beta + 1, ply + 1, false, sd);
      if (ShouldStopSearch())
        return 0;
      if (val >= beta)
        return beta;
    }
  }

  // Internal Iterative Deepening.
  if (depth >= 5 && tt_move == 0 && ply > 0) {
    AlphaBeta(board, depth - 2, alpha, beta, ply, false, sd);
    int temp_move = 0;
    ProbeTT(board.zobristKey, depth - 2, alpha, beta, temp_move);
    if (temp_move != 0)
      tt_move = temp_move;
  }

  MoveList list;
  GenerateMoves(board, list);
  SortMoves(list, board, tt_move, ply, sd, prev_piece, prev_to);

  int legal_moves = 0;
  int best_score = -50000;
  int flag = TT_ALPHA;
  Move best_move_struct = {0, 0, 0, 0, 0};

  for (int i = 0; i < list.count; i++) {
    Move move = list.moves[i];

    // Late Move Pruning for quiet moves at shallow depths.
    if (!in_check && depth <= 3 && legal_moves > 0 && !move.capture &&
        !move.promoted && !IsCastleMove(move)) {
      int lmp_limit = (depth == 1) ? 6 : (depth == 2 ? 10 : 14);
      if (legal_moves >= lmp_limit)
        continue;
    }

    Board copy = board;
    if (!MakeMove(copy, move, move.capture))
      continue;
    legal_moves++;

    // Check extension: extend search by 1 when we are in check (evasion).
    // Only extend at depth >= 2 to limit tree blowup.
    int extension = (in_check && depth >= 2) ? 1 : 0;
    int new_depth = depth - 1 + extension;

    int score;

    if (legal_moves == 1) {
      score = -AlphaBeta(copy, new_depth, -beta, -alpha, ply + 1, true, sd,
                         move.piece, move.to);
    } else {
      // Late Move Reduction (LMR).
      int R = 0;
      if (depth >= 3 && legal_moves >= 4 && !move.capture && !move.promoted &&
          !IsCastleMove(move) && extension == 0) {
        R = 1;
        if (depth > 5)
          R = 2;
        if (depth > 8 && legal_moves > 10)
          R += 1;
        // Reduce less for killer moves.
        int packed = PackMove(move);
        int ply_index = ClampPly(ply);
        if (sd.killer_moves[0][ply_index] == packed ||
            sd.killer_moves[1][ply_index] == packed)
          R = std::max(R - 1, 0);
      }

      int reduced_depth = new_depth - R;
      if (reduced_depth < 1)
        reduced_depth = 1;

      score = -AlphaBeta(copy, reduced_depth, -alpha - 1, -alpha, ply + 1, true, sd,
                         move.piece, move.to);

      // PVS re-search on fail-high.
      if (score > alpha && R > 0) {
        score = -AlphaBeta(copy, new_depth, -alpha - 1, -alpha, ply + 1, true, sd,
                           move.piece, move.to);
      }

      if (score > alpha && score < beta) {
        score = -AlphaBeta(copy, new_depth, -beta, -alpha, ply + 1, true, sd,
                           move.piece, move.to);
      }
    }

    if (ShouldStopSearch())
      return 0;

    if (score > best_score) {
      best_score = score;
      best_move_struct = move;
      if (score > alpha) {
        alpha = score;
        flag = TT_EXACT;
      }
    }

    if (alpha >= beta) {
      best_score = beta;
      flag = TT_BETA;

      if (!move.capture) {
        int ply_index = ClampPly(ply);
        int packed = PackMove(move);
        if (sd.killer_moves[0][ply_index] != packed) {
          sd.killer_moves[1][ply_index] = sd.killer_moves[0][ply_index];
          sd.killer_moves[0][ply_index] = packed;
        }

        // History heuristic.
        sd.history_moves[move.piece][move.to] += depth * depth;
        if (sd.history_moves[move.piece][move.to] > 7000) {
          for (int p = 0; p < 13; p++) {
            for (int s = 0; s < 64; s++) {
              sd.history_moves[p][s] /= 2;
            }
          }
        }

        // Counter-move heuristic.
        if (prev_piece != EMPTY && prev_to >= 0 && prev_to < 64) {
          sd.counter_moves[prev_piece][prev_to] = packed;
        }
      }
      break;
    }
  }

  if (legal_moves == 0) {
    return in_check ? (-49000 + ply) : 0;
  }

  StoreTT(board.zobristKey, depth, best_score, flag, PackMove(best_move_struct));
  return best_score;
}

struct RootResult {
  int best_score;
  Move best_move;
  int legal_moves;
};

void SearchRootSlice(Board &board, const std::vector<Move> &root_moves, int start,
                     int step, int curr_depth, int beta,
                     std::atomic<int> &shared_alpha,
                     std::atomic<bool> &root_cutoff, SearchData &sd,
                     RootResult &result, int skip_index) {
  result.best_score = -50000;
  result.best_move = {0, 0, 0, 0, 0};
  result.legal_moves = 0;

  for (int i = start; i < (int)root_moves.size(); i += step) {
    if (ShouldStopSearch() || root_cutoff.load(std::memory_order_relaxed))
      break;
    if (i == skip_index)
      continue;

    Board copy = board;
    Move move = root_moves[i];
    if (!MakeMove(copy, move, move.capture))
      continue;
    result.legal_moves++;

    int alpha_snapshot = shared_alpha.load(std::memory_order_relaxed);
    int score =
        -AlphaBeta(copy, curr_depth - 1, -alpha_snapshot - 1, -alpha_snapshot, 1,
                   true, sd);
    if (score > alpha_snapshot) {
      score = -AlphaBeta(copy, curr_depth - 1, -beta, -alpha_snapshot, 1, true,
                         sd);
    }

    if (ShouldStopSearch())
      break;

    if (score > result.best_score) {
      result.best_score = score;
      result.best_move = move;
    }

    int current_alpha = shared_alpha.load(std::memory_order_relaxed);
    while (score > current_alpha &&
           !shared_alpha.compare_exchange_weak(current_alpha, score,
                                               std::memory_order_relaxed)) {
    }

    if (score >= beta) {
      root_cutoff.store(true, std::memory_order_relaxed);
      break;
    }
  }
}

Move SearchPosition(Board &board, int depth) {
  auto start = std::chrono::high_resolution_clock::now();
  stop_search.store(false, std::memory_order_relaxed);

  time_movegen.store(0, std::memory_order_relaxed);
  time_makemove.store(0, std::memory_order_relaxed);
  time_eval.store(0, std::memory_order_relaxed);
  time_attack.store(0, std::memory_order_relaxed);

  SearchData main_sd;
  SearchData helper_sd;

  const bool use_helper = depth >= 6;

  Move best_move = {0, 0, 0, 0, 0};
  int best_score = -50000;

  for (int curr_depth = 1; curr_depth <= depth; curr_depth++) {
    int alpha = -50000;
    int beta = 50000;

    // Aspiration windows.
    if (curr_depth > 4) {
      alpha = best_score - 50;
      beta = best_score + 50;
    }

    int iteration_best_score = -50000;
    Move iteration_best_move = {0, 0, 0, 0, 0};
    bool research = false;
    int legal_moves = 0;
    int used_threads = 1;

  retry_search:
    iteration_best_score = -50000;
    legal_moves = 0;
    used_threads = 1;

    MoveList list;
    GenerateMoves(board, list);
    if (list.count == 0)
      break;

    int root_tt_move = (curr_depth > 1) ? PackMove(best_move) : 0;
    SortMoves(list, board, root_tt_move, 0, main_sd);
    std::vector<Move> root_moves;
    root_moves.reserve(list.count);

    for (int i = 0; i < list.count; i++) {
      root_moves.push_back(list.moves[i]);
    }

    if (root_moves.empty())
      break;

    if (use_helper && curr_depth >= 5 && root_moves.size() >= 3) {
      // Search first legal move serially to establish a useful alpha bound.
      int first_legal_index = -1;
      for (int i = 0; i < (int)root_moves.size(); i++) {
        Board first_copy = board;
        if (!MakeMove(first_copy, root_moves[i], root_moves[i].capture))
          continue;

        first_legal_index = i;
        legal_moves = 1;
        int first_score =
            -AlphaBeta(first_copy, curr_depth - 1, -beta, -alpha, 1, true,
                       main_sd);
        if (first_score > iteration_best_score) {
          iteration_best_score = first_score;
          iteration_best_move = root_moves[i];
        }
        if (first_score > alpha)
          alpha = first_score;

        if (!ShouldStopSearch() && first_score < beta) {
          unsigned int hw = std::thread::hardware_concurrency();
          int split_threads = static_cast<int>(hw ? hw : 2);
          if (split_threads > 4)
            split_threads = 4;
          if (split_threads < 2)
            split_threads = 2;
          if (split_threads > (int)root_moves.size())
            split_threads = static_cast<int>(root_moves.size());
          used_threads = split_threads;

          std::atomic<int> shared_alpha(alpha);
          std::atomic<bool> root_cutoff(false);
          std::vector<RootResult> results(split_threads);
          std::vector<SearchData> worker_sd(split_threads);
          std::vector<std::thread> threads;
          threads.reserve(split_threads - 1);

          for (int t = 1; t < split_threads; t++) {
            threads.emplace_back(SearchRootSlice, std::ref(board),
                                 std::cref(root_moves), t, split_threads,
                                 curr_depth, beta, std::ref(shared_alpha),
                                 std::ref(root_cutoff), std::ref(worker_sd[t]),
                                 std::ref(results[t]), first_legal_index);
          }

          SearchRootSlice(board, root_moves, 0, split_threads, curr_depth, beta,
                          shared_alpha, root_cutoff, main_sd, results[0],
                          first_legal_index);

          for (auto &th : threads)
            th.join();

          for (int t = 0; t < split_threads; t++) {
            legal_moves += results[t].legal_moves;
            if (results[t].best_score > iteration_best_score) {
              iteration_best_score = results[t].best_score;
              iteration_best_move = results[t].best_move;
            }
          }

          for (int t = 1; t < split_threads; t++) {
            helper_sd.nodes += worker_sd[t].nodes;
            helper_sd.q_nodes += worker_sd[t].q_nodes;
            helper_sd.tt_hits += worker_sd[t].tt_hits;
          }

          alpha = shared_alpha.load(std::memory_order_relaxed);
        }
        break;
      }
    } else {
      for (int i = 0; i < (int)root_moves.size(); i++) {
        Move move = root_moves[i];

        Board copy = board;
        if (!MakeMove(copy, move, move.capture))
          continue;
        legal_moves++;

        int score;
        if (legal_moves == 1) {
          score =
              -AlphaBeta(copy, curr_depth - 1, -beta, -alpha, 1, true, main_sd);
        } else {
          score = -AlphaBeta(copy, curr_depth - 1, -alpha - 1, -alpha, 1, true,
                             main_sd);
          if (score > alpha) {
            score = -AlphaBeta(copy, curr_depth - 1, -beta, -alpha, 1, true,
                               main_sd);
          }
        }

        if (ShouldStopSearch())
          break;

        if (score > iteration_best_score) {
          iteration_best_score = score;
          iteration_best_move = move;
        }
        if (score > alpha)
          alpha = score;
      }
    }

    if (ShouldStopSearch())
      break;
    if (legal_moves == 0)
      break;

    // Aspiration fail: re-search with full window once.
    if (curr_depth > 4 && !research &&
        (iteration_best_score <= best_score - 50 ||
         iteration_best_score >= best_score + 50)) {
      alpha = -50000;
      beta = 50000;
      research = true;
      goto retry_search;
    }

    if (iteration_best_score > -49000) {
      best_score = iteration_best_score;
      best_move = iteration_best_move;
    }

    auto right_now = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(right_now - start)
            .count();

    long long total_nodes = main_sd.nodes + main_sd.q_nodes;
    if (use_helper)
      total_nodes += helper_sd.nodes + helper_sd.q_nodes;

    long long nps = (duration > 0) ? (total_nodes * 1000 / duration) : 0;

    char cols[] = "abcdefgh";
    char rows[] = "12345678";

    std::cout << "info depth " << curr_depth << " score cp " << best_score
              << " nodes " << total_nodes << " nps " << nps << " time "
              << duration;
    std::cout << " pv " << cols[best_move.from % 8] << rows[best_move.from / 8]
              << cols[best_move.to % 8] << rows[best_move.to / 8];
    std::cout << " string Profiling(ms): MoveGen:" << (time_movegen / 1000000)
              << " MakeMove:" << (time_makemove / 1000000)
              << " Eval:" << (time_eval / 1000000)
              << " Attack:" << (time_attack / 1000000)
              << " TotalSearch:" << duration
              << " Threads: " << used_threads << std::endl;
  }

  stop_search.store(true, std::memory_order_relaxed);
  return best_move;
}

long Perft(Board &board, int depth) {
  if (depth == 0)
    return 1;

  long count = 0;
  MoveList list;
  GenerateMoves(board, list);

  for (int i = 0; i < list.count; i++) {
    Board copy = board;
    if (!MakeMove(copy, list.moves[i], list.moves[i].capture))
      continue;
    count += Perft(copy, depth - 1);
  }

  return count;
}
