#ifndef MOTOR_TRANSPOSITION_TABLE_HPP
#define MOTOR_TRANSPOSITION_TABLE_HPP

#include <vector>
#include <cstdint>

enum class Bound : std::uint8_t {
    INVALID,
    EXACT, // Type 1 - score is exact
    LOWER, // Type 2 - score is bigger than beta (fail-high) - Beta node
    UPPER  // Type 3 - score is lower than alpha (fail-low)  - Alpha node
};

struct TT_entry {
    Bound bound = Bound::INVALID; // 8 bits
    std::int8_t depth = 0;        // 8 bits
    std::int16_t score = 0;       // 16 bits
    std::int16_t static_eval = 0; // 16 bits
    chess_move tt_move = {};      // 16 bits
    std::int32_t age = 0;         // 32 bits
    std::uint32_t zobrist = 0;    // 32 bits
};

struct TT_cluster {
    std::array<TT_entry, 3> entries = {};
};

template<typename TT_CLUSTER>
class transposition_table {
public:
    explicit transposition_table(std::uint64_t size = 16 * 1024 * 1024) {
        resize(size);
    }

    void resize(const uint64_t byte_size) {
        bucket_count = byte_size / sizeof(TT_CLUSTER);
        tt_table.resize(bucket_count);
    }

    void clear() {
        tt_table = std::vector<TT_CLUSTER>(bucket_count);
        reset_age();
    }

    void prefetch(const std::uint64_t zobrist_hash) {
        __builtin_prefetch(&tt_table[get_index(zobrist_hash)]);
    }

    TT_CLUSTER & operator[](const std::uint64_t zobrist_hash) {
        return tt_table[get_index(zobrist_hash)];
    }

    const TT_CLUSTER & operator[](const std::uint64_t zobrist_hash) const {
        return tt_table[get_index(zobrist_hash)];
    }

    void store(const Bound flag, const int8_t depth, const int16_t best_score, const int16_t raw_eval, const chess_move best_move,
               const int16_t ply, const uint64_t zobrist_key) {

        const int16_t stored_score = [&] {
            if (best_score > 19'000) return static_cast<int16_t>(best_score + ply);
            if (best_score < -19'000) return static_cast<int16_t>(best_score - ply);
            return best_score;
        }();

        TT_CLUSTER &cluster = (*this)[zobrist_key];
        const auto stored_key = upper(zobrist_key);
        TT_entry new_entry{ flag, depth, stored_score, raw_eval, best_move, age, stored_key };

        // Find the first empty slot or the best slot to replace
        TT_entry *best_slot = &cluster.entries[0];
        for (TT_entry &entry : cluster.entries) {
            if (entry.bound == Bound::INVALID || entry.zobrist == 0 || entry.zobrist == stored_key) {
                best_slot = &entry;
                break;
            }
            if (static_cast<int>(entry.depth) - (this->age - entry.age) * 4 < static_cast<int>(best_slot->depth) - (this->age - best_slot->age) * 4) {
                best_slot = &entry;
            }
        }

        if (flag != Bound::EXACT && stored_key == best_slot->zobrist && depth < best_slot->depth - 3) {
            return;
        }

        *best_slot = new_entry;
    }

    TT_entry retrieve(const uint64_t zobrist_key, const int16_t ply) {
        TT_CLUSTER &cluster = (*this)[zobrist_key];

        for (TT_entry &entry : cluster.entries) {
            if (entry.zobrist == upper(zobrist_key)) {
                entry.score = [&] {
                    if (entry.score > 19'000) return static_cast<int16_t>(entry.score - ply);
                    if (entry.score < -19'000) return static_cast<int16_t>(entry.score + ply);
                    return entry.score;
                }();
                return entry;
            }
        }
        return TT_entry{}; // Return an empty TT_entry if not found
    }

    int get_index(const std::uint64_t zobrist_hash) {
        return static_cast<std::uint64_t>((static_cast<__int128>(zobrist_hash) * static_cast<__int128>(bucket_count)) >> 64);
    }

    uint32_t upper(const uint64_t zobrist_key) const {
        return (zobrist_key & 0xFFFFFFFF00000000) >> 32;
    }

    void reset_age() {
        age = 1;
    }

    void increase_age() {
        age++;
    }

private:
    std::vector<TT_CLUSTER> tt_table;
    std::uint64_t bucket_count;
    std::int32_t age = 1;
};

#endif //MOTOR_TRANSPOSITION_TABLE_HPP
