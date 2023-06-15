#ifndef MOTOR_TRANSPOSITION_TABLE_H
#define MOTOR_TRANSPOSITION_TABLE_H

#include "move_t.h"
#include <vector>


enum BOUND : uint8_t {
    INVALID,
    EXACT,  // Type 1 - score is exact
    LOWER, // Type 2 - score is bigger than beta (fail-high) - Beta node
    UPPER  // Type 3 - score is lower than alpha (fail-low)  - Alpha node
};

struct tt_info {
    uint64_t hash_key;
    int score;
    move_t best_move;
    int depth;
    BOUND type = INVALID;
};

// https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
// source: https://github.com/tensorflow/tensorflow/commit/a47a300185026fe7829990def9113bf3a5109fed
static uint64_t multiply_high_u64(uint64_t x, uint64_t y) {
#ifdef __SIZEOF_INT128__
    return (uint64_t)(((__uint128_t)x * (__uint128_t)y) >> 64);
#else
    // For platforms without int128 support, do it the long way.
        uint64_t x_lo = x & 0xffffffff;
        uint64_t x_hi = x >> 32;
        uint64_t buckets_lo = y & 0xffffffff;
        uint64_t buckets_hi = y >> 32;
        uint64_t prod_hi = x_hi * buckets_hi;
        uint64_t prod_lo = x_lo * buckets_lo;
        uint64_t prod_mid1 = x_hi * buckets_lo;
        uint64_t prod_mid2 = x_lo * buckets_hi;
        uint64_t carry = ((prod_mid1 & 0xffffffff) + (prod_mid2 & 0xffffffff) + (prod_lo >> 32)) >> 32;
        return prod_hi + (prod_mid1 >> 32) + (prod_mid2 >> 32) + carry;
#endif
}

class transposition_table {
    std::vector<tt_info> tt_table;
    uint64_t bucket_count;

public:
    transposition_table() {
        uint64_t size = 16 * 1024 * 1024;
        bucket_count = size / sizeof(tt_info);
        tt_table.resize(bucket_count);
    }

    void resize(const uint64_t size) {
        tt_table.resize(size);
        bucket_count = size / sizeof(tt_info);
        tt_table.resize(bucket_count);
    }

    void clear() {
        tt_table = std::vector<tt_info>(bucket_count);
    }

    void insert(uint64_t zobrist_hash, int score, move_t best_move, int depth, BOUND type) {
        tt_table[multiply_high_u64(zobrist_hash, bucket_count)] = {zobrist_hash, score, best_move, depth, type};
    }

    const tt_info & probe(const uint64_t zobrist_hash) const {
        return tt_table[multiply_high_u64(zobrist_hash, bucket_count)];
    }
} ttable;

#endif //MOTOR_TRANSPOSITION_TABLE_H
