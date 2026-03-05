#include "defs.h"
#include <cstdlib> // abs

// Optimization: Time tracking removed for performance, but variable kept for
// compilation compatibility
std::atomic<long long> time_makemove(0);

const int CastlePerm[64] = {13, 15, 15, 15, 12, 15, 15, 14, 15, 15, 15, 15, 15,
                            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                            15, 15, 15, 15, 7,  15, 15, 15, 3,  15, 15, 11};

int MakeMove(Board &board, Move move, int capture_flag) {
  int from = move.from;
  int to = move.to;
  int piece = move.piece;
  int side = board.side;
  int enemy = side ^ 1;

  // 1. Handle Captures
  // Done before moving piece so we don't overwrite squares[to] if it's a
  // capture
  if (move.capture) {
    int cap_piece = EMPTY;
    int cap_sq = to;

    // Check for En Passant
    if ((piece == wP || piece == bP) && to == board.enPas) {
      cap_piece = (side == WHITE) ? bP : wP;
      cap_sq = (side == WHITE) ? to - 8 : to + 8;
    } else {
      // Normal Capture
      cap_piece = board.squares[to];
    }

    if (cap_piece != EMPTY) {
      // Remove captured piece
      POP_BIT(board.pieces[cap_piece], cap_sq);
      POP_BIT(board.occupancies[enemy], cap_sq);
      POP_BIT(board.occupancies[BOTH], cap_sq);
      board.zobristKey ^= piece_keys[cap_piece][cap_sq];
      board.squares[cap_sq] = EMPTY;
    }
  }

  // 2. Move the Piece
  // Remove from source
  POP_BIT(board.pieces[piece], from);
  POP_BIT(board.occupancies[side], from);
  POP_BIT(board.occupancies[BOTH], from);
  board.zobristKey ^= piece_keys[piece][from];
  board.squares[from] = EMPTY;

  // Add to destination
  SET_BIT(board.pieces[piece], to);
  SET_BIT(board.occupancies[side], to);
  SET_BIT(board.occupancies[BOTH], to);
  board.zobristKey ^= piece_keys[piece][to];
  board.squares[to] = piece;

  if (piece == wK)
    board.kingSq[WHITE] = to;
  else if (piece == bK)
    board.kingSq[BLACK] = to;

  // 3. Handle Promotions
  if (move.promoted) {
    int prom = move.promoted;

    // Remove the Pawn that just moved to 'to'
    POP_BIT(board.pieces[piece], to);
    board.zobristKey ^= piece_keys[piece][to];
    // Note: Occupancies are already set (it's still a piece of same color)

    // Add Promoted Piece
    SET_BIT(board.pieces[prom], to);
    board.zobristKey ^= piece_keys[prom][to];
    board.squares[to] = prom;
  }

  // 4. Update En Passant
  // Remove old En Passant hash
  if (board.enPas != NO_SQ) {
    board.zobristKey ^= piece_keys[12][board.enPas]; // 12 used for EnPas keys
  }
  board.enPas = NO_SQ;

  // Set New En Passant if Double Push
  if (piece == wP && (to - from == 16)) {
    board.enPas = from + 8;
    board.zobristKey ^= piece_keys[12][board.enPas];
  } else if (piece == bP && (from - to == 16)) {
    board.enPas = from - 8;
    board.zobristKey ^= piece_keys[12][board.enPas];
  }

  // 5. Update Castling Rights
  board.zobristKey ^= castle_keys[board.castle]; // Remove old
  board.castle &= CastlePerm[from];
  board.castle &= CastlePerm[to];
  board.zobristKey ^= castle_keys[board.castle]; // Add new

  // 6. Move Rook if Castling
  if (piece == wK || piece == bK) {
    if (abs(to - from) == 2) {
      if (to == G1) { // White King Side -> Rook H1 to F1
        POP_BIT(board.pieces[wR], H1);
        POP_BIT(board.occupancies[WHITE], H1);
        POP_BIT(board.occupancies[BOTH], H1);
        board.zobristKey ^= piece_keys[wR][H1];
        board.squares[H1] = EMPTY;

        SET_BIT(board.pieces[wR], F1);
        SET_BIT(board.occupancies[WHITE], F1);
        SET_BIT(board.occupancies[BOTH], F1);
        board.zobristKey ^= piece_keys[wR][F1];
        board.squares[F1] = wR;
      } else if (to == C1) { // White Queen Side -> Rook A1 to D1
        POP_BIT(board.pieces[wR], A1);
        POP_BIT(board.occupancies[WHITE], A1);
        POP_BIT(board.occupancies[BOTH], A1);
        board.zobristKey ^= piece_keys[wR][A1];
        board.squares[A1] = EMPTY;

        SET_BIT(board.pieces[wR], D1);
        SET_BIT(board.occupancies[WHITE], D1);
        SET_BIT(board.occupancies[BOTH], D1);
        board.zobristKey ^= piece_keys[wR][D1];
        board.squares[D1] = wR;
      } else if (to == G8) { // Black King Side -> Rook H8 to F8
        POP_BIT(board.pieces[bR], H8);
        POP_BIT(board.occupancies[BLACK], H8);
        POP_BIT(board.occupancies[BOTH], H8);
        board.zobristKey ^= piece_keys[bR][H8];
        board.squares[H8] = EMPTY;

        SET_BIT(board.pieces[bR], F8);
        SET_BIT(board.occupancies[BLACK], F8);
        SET_BIT(board.occupancies[BOTH], F8);
        board.zobristKey ^= piece_keys[bR][F8];
        board.squares[F8] = bR;
      } else if (to == C8) { // Black Queen Side -> Rook A8 to D8
        POP_BIT(board.pieces[bR], A8);
        POP_BIT(board.occupancies[BLACK], A8);
        POP_BIT(board.occupancies[BOTH], A8);
        board.zobristKey ^= piece_keys[bR][A8];
        board.squares[A8] = EMPTY;

        SET_BIT(board.pieces[bR], D8);
        SET_BIT(board.occupancies[BLACK], D8);
        SET_BIT(board.occupancies[BOTH], D8);
        board.zobristKey ^= piece_keys[bR][D8];
        board.squares[D8] = bR;
      }
    }
  }

  // 7. Update Side and Hash
  board.side ^= 1;
  board.zobristKey ^= side_key;

  // 8. Check Legality (King in Check)
  // We check if the king of the side that just moved (now 'enemy' relative to
  // board.side, or 'side' variable) is attacked by the side to move
  // (board.side).

  int king_sq = board.kingSq[side];

  if (king_sq != -1) {
    if (IsSquareAttacked(king_sq, board.side, board)) {
      return 0; // Illegal move
    }
  }

  return 1;
}
