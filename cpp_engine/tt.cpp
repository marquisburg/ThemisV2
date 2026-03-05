#include "defs.h"

static_assert((TT_SIZE & (TT_SIZE - 1)) == 0, "TT_SIZE must be a power of two");
constexpr int TT_BUCKET_SIZE = 4;
static_assert((TT_SIZE % TT_BUCKET_SIZE) == 0,
              "TT_SIZE must be divisible by TT_BUCKET_SIZE");
constexpr int TT_MASK = TT_SIZE - 1;

inline U64 PackTTData(int move, int score, int depth, int flags) {
    U64 packed = 0ULL;
    packed |= static_cast<U64>(static_cast<unsigned int>(move) & 0xFFFFU);
    packed |= static_cast<U64>(static_cast<unsigned int>(depth) & 0xFFU) << 16;
    packed |= static_cast<U64>(static_cast<unsigned int>(flags) & 0xFFU) << 24;
    packed |= static_cast<U64>(static_cast<unsigned int>(score)) << 32;
    return packed;
}

inline void UnpackTTData(U64 packed, int& move, int& score, int& depth, int& flags) {
    move = static_cast<int>(packed & 0xFFFFULL);
    depth = static_cast<int>((packed >> 16) & 0xFFULL);
    flags = static_cast<int>((packed >> 24) & 0xFFULL);
    score = static_cast<int>(packed >> 32);
}

// Returns score if found and cutoff possible, else -50001 (Invalid)
int ProbeTT(U64 key, int depth, int alpha, int beta, int& move) {
    int index = static_cast<int>(key & TT_MASK);
    int bucket = index & ~(TT_BUCKET_SIZE - 1);

    move = 0;
    for (int i = 0; i < TT_BUCKET_SIZE; i++) {
        TTEntry* entry = &TranspositionTable[bucket + i];
        U64 entry_key = entry->key.load(std::memory_order_acquire);
        if (entry_key != key) continue;

        int entry_move, entry_score, entry_depth, entry_flags;
        U64 packed = entry->data.load(std::memory_order_relaxed);
        UnpackTTData(packed, entry_move, entry_score, entry_depth, entry_flags);
        move = entry_move;

        if (entry_depth >= depth) {
            if (entry_flags == TT_EXACT) return entry_score;
            if (entry_flags == TT_ALPHA && entry_score <= alpha) return alpha;
            if (entry_flags == TT_BETA && entry_score >= beta) return beta;
        }
    }

    return -50001; // Not found code
}

void StoreTT(U64 key, int depth, int score, int flags, int move) {
    int index = static_cast<int>(key & TT_MASK);
    int bucket = index & ~(TT_BUCKET_SIZE - 1);

    int replace_slot = bucket;
    int shallowest_depth = 9999;

    for (int i = 0; i < TT_BUCKET_SIZE; i++) {
        TTEntry* entry = &TranspositionTable[bucket + i];
        U64 entry_key = entry->key.load(std::memory_order_relaxed);

        if (entry_key == key) {
            replace_slot = bucket + i;
            shallowest_depth = -1;
            break;
        }

        if (entry_key == 0ULL) {
            replace_slot = bucket + i;
            shallowest_depth = -1;
            break;
        }

        int entry_move, entry_score, entry_depth, entry_flags;
        U64 packed = entry->data.load(std::memory_order_relaxed);
        UnpackTTData(packed, entry_move, entry_score, entry_depth, entry_flags);
        if (entry_depth < shallowest_depth) {
            shallowest_depth = entry_depth;
            replace_slot = bucket + i;
        }
    }

    TTEntry* target = &TranspositionTable[replace_slot];
    U64 packed = PackTTData(move, score, depth, flags);
    target->data.store(packed, std::memory_order_relaxed);
    target->key.store(key, std::memory_order_release);
}
