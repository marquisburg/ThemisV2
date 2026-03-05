#include "defs.h"
#include <cstdlib>

U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];

U64 piece_keys[13][64];
U64 side_key;
U64 castle_keys[16];
TTEntry *TranspositionTable = NULL;

U64 FileMasks[8];
U64 RankMasks[8];
U64 BlackPassedMask[64];
U64 WhitePassedMask[64];
U64 IsolatedMask[64];

const U64 not_a_file = 18374403900871474942ULL;
const U64 not_h_file = 9187201950435737471ULL;
const U64 not_hg_file = 4557430888798830399ULL;
const U64 not_ab_file = 18229723555195321596ULL;

U64 Rand64() {
  return (U64)rand() | ((U64)rand() << 15) | ((U64)rand() << 30) |
         ((U64)rand() << 45) | ((U64)rand() << 60);
}

void InitHash() {
  for (int p = 0; p < 13; p++) {
    for (int sq = 0; sq < 64; sq++) {
      piece_keys[p][sq] = Rand64();
    }
  }
  side_key = Rand64();
  for (int k = 0; k < 16; k++)
    castle_keys[k] = Rand64();
}

void ClearTT() {
  for (int i = 0; i < TT_SIZE; i++) {
    TranspositionTable[i].data.store(0, std::memory_order_relaxed);
    TranspositionTable[i].key.store(0, std::memory_order_relaxed);
  }
}

void InitTT() {
  TranspositionTable = new TTEntry[TT_SIZE];
  ClearTT();
}

U64 MaskPawnAttacks(int side, int sq) {
  U64 attacks = 0ULL;
  U64 bitboard = 0ULL;
  SET_BIT(bitboard, sq);

  if (side == WHITE) {
    if ((bitboard << 7) & not_h_file)
      attacks |= (bitboard << 7);
    if ((bitboard << 9) & not_a_file)
      attacks |= (bitboard << 9);
  } else {
    if ((bitboard >> 9) & not_h_file)
      attacks |= (bitboard >> 9);
    if ((bitboard >> 7) & not_a_file)
      attacks |= (bitboard >> 7);
  }
  return attacks;
}

U64 MaskKnightAttacks(int sq) {
  U64 attacks = 0ULL;
  U64 bitboard = 0ULL;
  SET_BIT(bitboard, sq);

  if ((bitboard >> 17) & not_h_file)
    attacks |= (bitboard >> 17);
  if ((bitboard >> 15) & not_a_file)
    attacks |= (bitboard >> 15);
  if ((bitboard >> 10) & not_hg_file)
    attacks |= (bitboard >> 10);
  if ((bitboard >> 6) & not_ab_file)
    attacks |= (bitboard >> 6);

  if ((bitboard << 17) & not_a_file)
    attacks |= (bitboard << 17);
  if ((bitboard << 15) & not_h_file)
    attacks |= (bitboard << 15);
  if ((bitboard << 10) & not_ab_file)
    attacks |= (bitboard << 10);
  if ((bitboard << 6) & not_hg_file)
    attacks |= (bitboard << 6);

  return attacks;
}

U64 MaskKingAttacks(int sq) {
  U64 attacks = 0ULL;
  U64 bitboard = 0ULL;
  SET_BIT(bitboard, sq);

  if (bitboard >> 8)
    attacks |= (bitboard >> 8);
  if ((bitboard >> 9) & not_h_file)
    attacks |= (bitboard >> 9);
  if ((bitboard >> 7) & not_a_file)
    attacks |= (bitboard >> 7);
  if ((bitboard >> 1) & not_h_file)
    attacks |= (bitboard >> 1);

  if (bitboard << 8)
    attacks |= (bitboard << 8);
  if ((bitboard << 9) & not_a_file)
    attacks |= (bitboard << 9);
  if ((bitboard << 7) & not_h_file)
    attacks |= (bitboard << 7);
  if ((bitboard << 1) & not_a_file)
    attacks |= (bitboard << 1);

  return attacks;
}

void InitEvalMasks() {
  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 8; f++) {
      int sq = r * 8 + f;
      FileMasks[f] |= (1ULL << sq);
      RankMasks[r] |= (1ULL << sq);
    }
  }

  for (int sq = 0; sq < 64; sq++) {
    int r = sq / 8;
    int f = sq % 8;

    // White Passed (Squares in front)
    for (int i = r + 1; i < 8; i++) {
      WhitePassedMask[sq] |= (1ULL << (i * 8 + f));
      if (f > 0)
        WhitePassedMask[sq] |= (1ULL << (i * 8 + f - 1));
      if (f < 7)
        WhitePassedMask[sq] |= (1ULL << (i * 8 + f + 1));
    }

    // Black Passed (Squares behind? No, in front from Black perspective -> Rank
    // 0)
    for (int i = r - 1; i >= 0; i--) {
      BlackPassedMask[sq] |= (1ULL << (i * 8 + f));
      if (f > 0)
        BlackPassedMask[sq] |= (1ULL << (i * 8 + f - 1));
      if (f < 7)
        BlackPassedMask[sq] |= (1ULL << (i * 8 + f + 1));
    }

    // Isolated
    if (f > 0)
      IsolatedMask[sq] |= FileMasks[f - 1];
    if (f < 7)
      IsolatedMask[sq] |= FileMasks[f + 1];
  }
}

// Magic numbers initialization
Magic rookMagics[64];
Magic bishopMagics[64];
U64 attackTable[100000]; // ~800KB for attacks

// Helper to get random U64 with few bits set
U64 RandomU64FewBits() {
  return Rand64() & Rand64() & Rand64(); // Sparse
}

// Helper: Count bits
int CountBits(U64 b) { return __builtin_popcountll(b); }

// Helper: Set occupancy from index
U64 SetOccupancy(int index, int bits_in_mask, U64 attack_mask) {
  U64 occupancy = 0ULL;
  for (int count = 0; count < bits_in_mask; count++) {
    int square = __builtin_ctzll(attack_mask);
    POP_BIT(attack_mask, square);
    if (index & (1 << count)) {
      occupancy |= (1ULL << square);
    }
  }
  return occupancy;
}

// Slow (On-The-Fly) Attack Generation for Initialization
U64 GetBishopAttacksSlow(int sq, U64 block) {
  U64 attacks = 0ULL;
  int r, f;
  int tr = sq / 8;
  int tf = sq % 8;
  for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
    SET_BIT(attacks, (r * 8 + f));
    if (GET_BIT(block, (r * 8 + f)))
      break;
  }
  for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
    SET_BIT(attacks, (r * 8 + f));
    if (GET_BIT(block, (r * 8 + f)))
      break;
  }
  for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
    SET_BIT(attacks, (r * 8 + f));
    if (GET_BIT(block, (r * 8 + f)))
      break;
  }
  for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
    SET_BIT(attacks, (r * 8 + f));
    if (GET_BIT(block, (r * 8 + f)))
      break;
  }
  return attacks;
}

U64 GetRookAttacksSlow(int sq, U64 block) {
  U64 attacks = 0ULL;
  int r, f;
  int tr = sq / 8;
  int tf = sq % 8;
  for (r = tr + 1; r <= 7; r++) {
    SET_BIT(attacks, (r * 8 + tf));
    if (GET_BIT(block, (r * 8 + tf)))
      break;
  }
  for (r = tr - 1; r >= 0; r--) {
    SET_BIT(attacks, (r * 8 + tf));
    if (GET_BIT(block, (r * 8 + tf)))
      break;
  }
  for (f = tf + 1; f <= 7; f++) {
    SET_BIT(attacks, (tr * 8 + f));
    if (GET_BIT(block, (tr * 8 + f)))
      break;
  }
  for (f = tf - 1; f >= 0; f--) {
    SET_BIT(attacks, (tr * 8 + f));
    if (GET_BIT(block, (tr * 8 + f)))
      break;
  }
  return attacks;
}

// Magic Mask Generation
U64 MaskBishopMagic(int sq) {
  U64 attacks = 0ULL;
  int r, f;
  int tr = sq / 8;
  int tf = sq % 8;
  for (r = tr + 1, f = tf + 1; r < 7 && f < 7; r++, f++)
    SET_BIT(attacks, (r * 8 + f));
  for (r = tr - 1, f = tf + 1; r > 0 && f < 7; r--, f++)
    SET_BIT(attacks, (r * 8 + f));
  for (r = tr + 1, f = tf - 1; r < 7 && f > 0; r++, f--)
    SET_BIT(attacks, (r * 8 + f));
  for (r = tr - 1, f = tf - 1; r > 0 && f > 0; r--, f--)
    SET_BIT(attacks, (r * 8 + f));
  return attacks;
}

U64 MaskRookMagic(int sq) {
  U64 attacks = 0ULL;
  int r, f;
  int tr = sq / 8;
  int tf = sq % 8;
  for (r = tr + 1; r < 7; r++)
    SET_BIT(attacks, (r * 8 + tf));
  for (r = tr - 1; r > 0; r--)
    SET_BIT(attacks, (r * 8 + tf));
  for (f = tf + 1; f < 7; f++)
    SET_BIT(attacks, (tr * 8 + f));
  for (f = tf - 1; f > 0; f--)
    SET_BIT(attacks, (tr * 8 + f));
  return attacks;
}

// Find Magic Number
U64 FindMagic(int sq, int relevant_bits, int is_bishop) {
  U64 occupancies[4096];
  U64 attacks[4096];
  U64 used_attacks[4096];
  U64 mask = is_bishop ? MaskBishopMagic(sq) : MaskRookMagic(sq);
  int num_occupancies = 1 << relevant_bits;

  for (int i = 0; i < num_occupancies; i++) {
    occupancies[i] = SetOccupancy(i, relevant_bits, mask);
    attacks[i] = is_bishop ? GetBishopAttacksSlow(sq, occupancies[i])
                           : GetRookAttacksSlow(sq, occupancies[i]);
  }

  for (int k = 0; k < 100000000; k++) {
    U64 magic_candidate = RandomU64FewBits();
    if (CountBits((mask * magic_candidate) & 0xFF00000000000000ULL) < 6)
      continue;

    for (int i = 0; i < 4096; i++)
      used_attacks[i] = 0ULL;

    bool fail = false;
    for (int i = 0; i < num_occupancies; i++) {
      unsigned int magic_index =
          (int)((occupancies[i] * magic_candidate) >> (64 - relevant_bits));
      if (used_attacks[magic_index] == 0ULL) {
        used_attacks[magic_index] = attacks[i];
      } else if (used_attacks[magic_index] != attacks[i]) {
        fail = true;
        break;
      }
    }
    if (!fail)
      return magic_candidate;
  }
  std::cout << "Magic generation failed for square " << sq << std::endl;
  return 0ULL;
}

void InitMagic() {
  // Attack buffer management.
  // We reused `attackTable` but it needs to be big enough.
  // But pointers are set into dynamically allocated memory or a big static
  // array. Let's use a big static array pointer and increment. Total size
  // ~107KB for Bishop, ~2MB for Rook? Max bits for rook is 12 -> 4096 entries.
  // 64 rooks * 4096 * 8 bytes = 2MB. Max bits for bishop is 9 -> 512 entries.
  // 64 bishops * 512 * 8 bytes = 256KB. Total ~2.3MB.

  // We declared attackTable[100000] which is 800KB. Too small.
  // Let's allocate dynamically.

  int rook_relevant_bits[64] = {
      12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10, 10, 10, 11,
      11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
      11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
      11, 10, 10, 10, 10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12};

  int bishop_relevant_bits[64] = {
      6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7,
      5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7,
      7, 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6};

  for (int sq = 0; sq < 64; sq++) {
    // Bishops
    bishopMagics[sq].mask = MaskBishopMagic(sq);
    bishopMagics[sq].shift = 64 - bishop_relevant_bits[sq];
    bishopMagics[sq].attacks = new U64[1 << bishop_relevant_bits[sq]];
    bishopMagics[sq].magic = FindMagic(sq, bishop_relevant_bits[sq], 1);

    int count = 1 << bishop_relevant_bits[sq];
    for (int i = 0; i < count; i++) {
      U64 occupancy =
          SetOccupancy(i, bishop_relevant_bits[sq], bishopMagics[sq].mask);
      int magic_index =
          (int)((occupancy * bishopMagics[sq].magic) >> bishopMagics[sq].shift);
      bishopMagics[sq].attacks[magic_index] =
          GetBishopAttacksSlow(sq, occupancy);
    }

    // Rooks
    rookMagics[sq].mask = MaskRookMagic(sq);
    rookMagics[sq].shift = 64 - rook_relevant_bits[sq];
    rookMagics[sq].attacks = new U64[1 << rook_relevant_bits[sq]];
    rookMagics[sq].magic = FindMagic(sq, rook_relevant_bits[sq], 0);

    count = 1 << rook_relevant_bits[sq];
    for (int i = 0; i < count; i++) {
      U64 occupancy =
          SetOccupancy(i, rook_relevant_bits[sq], rookMagics[sq].mask);
      int magic_index =
          (int)((occupancy * rookMagics[sq].magic) >> rookMagics[sq].shift);
      rookMagics[sq].attacks[magic_index] = GetRookAttacksSlow(sq, occupancy);
    }
  }
  std::cout << "Magic Bitboards Initialized" << std::endl;
}

void InitAll() {
  InitHash();
  InitTT();
  InitMagic(); // Initialize magics first or concurrent with others
  InitEvalMasks();
  for (int sq = 0; sq < 64; sq++) {
    pawn_attacks[WHITE][sq] = MaskPawnAttacks(WHITE, sq);
    pawn_attacks[BLACK][sq] = MaskPawnAttacks(BLACK, sq);
    knight_attacks[sq] = MaskKnightAttacks(sq);
    king_attacks[sq] = MaskKingAttacks(sq);
  }
}
