#include "defs.h"
#include <cstring>
#include <iostream>

Board::Board() {
  memset(pieces, 0, sizeof(pieces));
  memset(occupancies, 0, sizeof(occupancies));
  for (int i = 0; i < 64; ++i)
    squares[i] = EMPTY;
  kingSq[WHITE] = E1;
  kingSq[BLACK] = E8;
  side = WHITE;
  enPas = NO_SQ;
  castle = 0;
}

void Board::UpdateOccupancies() {
  occupancies[WHITE] = 0;
  occupancies[BLACK] = 0;
  occupancies[BOTH] = 0;

  for (int p = wP; p <= wK; p++)
    occupancies[WHITE] |= pieces[p];
  for (int p = bP; p <= bK; p++)
    occupancies[BLACK] |= pieces[p];

  occupancies[BOTH] = occupancies[WHITE] | occupancies[BLACK];
}

void Board::ParseFen(const char *fen) {
  // Reset board
  memset(pieces, 0, sizeof(pieces));
  memset(occupancies, 0, sizeof(occupancies));
  for (int i = 0; i < 64; ++i)
    squares[i] = EMPTY;
  kingSq[WHITE] = -1;
  kingSq[BLACK] = -1;
  side = WHITE;
  enPas = NO_SQ;
  castle = 0;

  int rank = 7;
  int file = 0;

  while ((rank >= 0) && *fen) {
    int count = 1;
    switch (*fen) {
    case 'p':
      SET_BIT(pieces[bP], (rank * 8 + file));
      break;
    case 'n':
      SET_BIT(pieces[bN], (rank * 8 + file));
      break;
    case 'b':
      SET_BIT(pieces[bB], (rank * 8 + file));
      break;
    case 'r':
      SET_BIT(pieces[bR], (rank * 8 + file));
      break;
    case 'q':
      SET_BIT(pieces[bQ], (rank * 8 + file));
      break;
    case 'k':
      SET_BIT(pieces[bK], (rank * 8 + file));
      kingSq[BLACK] = rank * 8 + file;
      break;
    case 'P':
      SET_BIT(pieces[wP], (rank * 8 + file));
      break;
    case 'N':
      SET_BIT(pieces[wN], (rank * 8 + file));
      break;
    case 'B':
      SET_BIT(pieces[wB], (rank * 8 + file));
      break;
    case 'R':
      SET_BIT(pieces[wR], (rank * 8 + file));
      break;
    case 'Q':
      SET_BIT(pieces[wQ], (rank * 8 + file));
      break;
    case 'K':
      SET_BIT(pieces[wK], (rank * 8 + file));
      kingSq[WHITE] = rank * 8 + file;
      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
      count = *fen - '0';
      file += (count - 1); // Adjust for loop increment
      break;

    case '/':
    case ' ':
      rank--;
      file = -1; // Reset file
      break;
    }
    file++;
    fen++;
  }

  // Side to move
  // Note: Simple parser, assumes standard FEN structure roughly
  while (*fen == ' ')
    fen++;
  if (*fen == 'w')
    side = WHITE;
  else
    side = BLACK;

  fen += 2;
  // Castling
  while (*fen != ' ') {
    switch (*fen) {
    case 'K':
      castle |= WKCA;
      break;
    case 'Q':
      castle |= WQCA;
      break;
    case 'k':
      castle |= BKCA;
      break;
    case 'q':
      castle |= BQCA;
      break;
    case '-':
      break;
    }
    fen++;
  }

  UpdateOccupancies();

  // Populate squares from bitboards
  for (int p = wP; p <= bK; p++) {
    U64 bb = pieces[p];
    while (bb) {
      int sq = __builtin_ctzll(bb);
      POP_BIT(bb, sq);

      squares[sq] = p;
      if (p == wK)
        kingSq[WHITE] = sq;
      else if (p == bK)
        kingSq[BLACK] = sq;
    }
  }

  zobristKey = GenerateZobristHash();
}

U64 Board::GenerateZobristHash() {
  U64 final_key = 0;

  // Pieces
  for (int p = wP; p <= bK; p++) {
    U64 bb = pieces[p];
    while (bb) {
      int sq = __builtin_ctzll(bb);
      POP_BIT(bb, sq);
      final_key ^= piece_keys[p][sq];
    }
  }

  // Side
  if (side == WHITE)
    final_key ^= side_key;

  // Castle
  final_key ^= castle_keys[castle];

  // EnPas
  if (enPas != NO_SQ) {
    final_key ^= piece_keys[12][enPas];
  }

  return final_key;
}

void Board::Print() {
  std::cout << "\n";
  for (int rank = 7; rank >= 0; rank--) {
    std::cout << rank + 1 << "  ";
    for (int file = 0; file < 8; file++) {
      int sq = rank * 8 + file;
      int piece = squares[sq];

      char ascii_pieces[] = "PNBRQKpnbrqk";
      if (piece != EMPTY) {
        std::cout << ascii_pieces[piece] << " ";
      } else {
        std::cout << ". ";
      }
    }
    std::cout << "\n";
  }
  std::cout << "\n   a b c d e f g h\n";
  std::cout << "   Side: " << (side == WHITE ? "White" : "Black") << "\n";
}
