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
    explicit transposition_table(const std::uint64_t size = 16 * 1024 * 1024) {
        resize(size);
    }

    void resize(const std::uint64_t byte_size) {
        this->cluster_count = byte_size / sizeof(TT_CLUSTER);
        tt_table.resize(this->cluster_count);
    }

    void clear() {
        tt_table = std::vector<TT_CLUSTER>(cluster_count);
        reset_age();
    }

    void prefetch(const std::uint64_t zobrist_hash) {
        __builtin_prefetch(&tt_table[get_index(zobrist_hash)]);
    }

    void store(const Bound flag, const std::int8_t depth, const std::int16_t best_score, const std::int16_t raw_eval,
               const chess_move best_move, const std::int16_t ply, const std::uint64_t zobrist_key) {

        const int16_t stored_score = [&] {
            if (best_score > 19'000) return static_cast<int16_t>(best_score + ply);
            if (best_score < -19'000) return static_cast<int16_t>(best_score - ply);
            return best_score;
        }();

        TT_CLUSTER &cluster = tt_table[get_index(zobrist_key)];
        const auto stored_key = upper(zobrist_key);
        TT_entry new_entry{ flag, depth, stored_score, raw_eval, best_move, age, stored_key };

        TT_entry *best_slot = &cluster.entries[0];
        int best_relevance = static_cast<int>(best_slot->depth) + 4 * static_cast<int>(best_slot->age);

        for (TT_entry &entry : cluster.entries) {
            if (entry.zobrist == 0 || entry.zobrist == stored_key) {
                best_slot = &entry;
                break;
            }

            int relevance = static_cast<int>(entry.depth) + 4 * static_cast<int>(entry.age);
            if (relevance < best_relevance) {
                best_slot = &entry;
                best_relevance = relevance;
            }
        }

        if (flag != Bound::EXACT && stored_key == best_slot->zobrist && depth < best_slot->depth - 3) {
            return;
        }

        *best_slot = new_entry;
    }

    TT_entry retrieve(const std::uint64_t zobrist_key, const std::int16_t ply) {
        TT_CLUSTER &cluster = tt_table[get_index(zobrist_key)];

        for (auto &entry : cluster.entries) {
            if (entry.zobrist == upper(zobrist_key)) {
                entry.score = [&] {
                    if (entry.score > 19'000) return static_cast<int16_t>(entry.score - ply);
                    if (entry.score < -19'000) return static_cast<int16_t>(entry.score + ply);
                    return entry.score;
                }();
                return entry;
            }
        }
        return TT_entry{};
    }

    int get_index(const std::uint64_t zobrist_hash) {
        return static_cast<std::uint64_t>((static_cast<__int128>(zobrist_hash) * static_cast<__int128>(cluster_count)) >> 64);
    }

    std::uint32_t upper(const std::uint64_t zobrist_key) const {
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
    std::uint64_t cluster_count;
    std::int32_t age = 1;
};

#endif //MOTOR_TRANSPOSITION_TABLE_HPP
