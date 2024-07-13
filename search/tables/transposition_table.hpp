#ifndef MOTOR_TRANSPOSITION_TABLE_HPP
#define MOTOR_TRANSPOSITION_TABLE_HPP

#include <vector>
#include <cstdint>

enum class Bound : std::uint8_t {
    EXACT, // Type 1 - score is exact
    LOWER, // Type 2 - score is bigger than beta (fail-high) - Beta node
    UPPER  // Type 3 - score is lower than alpha (fail-low)  - Alpha node
};

struct TT_entry {
    Bound bound;            // 8 bits
    std::int8_t depth;      // 8 bits
    std::int16_t score;     // 16 bits
    std::int16_t static_eval; // 16 bits
    chess_move tt_move;     // 16 bits
    std::uint32_t age;      // 32 bits (but can be less, and unused for now)
    std::uint32_t zobrist;  // 32 bits
};

template<typename TT_ENTRY>
class transposition_table {
public:
    explicit transposition_table(std::uint64_t size = 16 * 1024 * 1024) {
        resize(size);
    }

    void resize(const uint64_t byte_size) {
        bucket_count = byte_size / sizeof(TT_ENTRY);
        mask = bucket_count - 1;
        tt_table.resize(bucket_count);
    }

    void clear() {
        tt_table = std::vector<TT_ENTRY>(bucket_count);
    }

    void prefetch(const std::uint64_t zobrist_hash) {
        __builtin_prefetch(&tt_table[zobrist_hash & mask]);
    }

    TT_ENTRY & operator[](const std::uint64_t zobrist_hash) {
        return tt_table[zobrist_hash & mask];
    }

    const TT_ENTRY & operator[](const std::uint64_t zobrist_hash) const {
        std::uint64_t index = static_cast<std::uint64_t>((static_cast<__int128>(zobrist_hash) * static_cast<__int128>(bucket_count)) >> 64);
        return tt_table[index];
    }

    void store(const Bound flag, const int8_t depth, const int16_t best_score, const int16_t raw_eval, const chess_move best_move,
        const int16_t ply, const uint64_t zobrist_key) {

        const int16_t stored_score = [&] {
            if (best_score > 19'000) return static_cast<int16_t>(best_score + ply);
            if (best_score < -19'000) return static_cast<int16_t>(best_score - ply);
            return best_score;
        }();
        tt[zobrist_key] = { flag, depth, stored_score, raw_eval, best_move, 0, upper(zobrist_key) };
    }

    TT_ENTRY retrieve(const uint64_t zobrist_key, const int16_t ply) {
        TT_ENTRY tt_entry = tt[zobrist_key];

        tt_entry.score = [&] {
            if (tt_entry.score > 19'000) return static_cast<int16_t>(tt_entry.score - ply);
            if (tt_entry.score < -19'000) return static_cast<int16_t>(tt_entry.score + ply);
            return tt_entry.score;
        }();
        return tt_entry;
    }

    uint32_t upper(const uint64_t zobrist_key) const {
        return (zobrist_key & 0xFFFFFFFF00000000) >> 32;
    }

private:
    std::vector<TT_ENTRY> tt_table;
    std::uint64_t bucket_count;
    std::uint64_t mask; // mask == bucket_count - 1
};

#endif //MOTOR_TRANSPOSITION_TABLE_HPP
