#ifndef MOTOR_TRANSPOSITION_TABLE_HPP
#define MOTOR_TRANSPOSITION_TABLE_HPP

#include <vector>
#include <cstdint>

template<typename TT_ENTRY>
class transposition_table {
public:
    explicit transposition_table(std::uint64_t size = 32 * 1024 * 1024) {
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

    TT_ENTRY & operator[](const std::uint64_t zobrist_hash) {
        return tt_table[zobrist_hash & mask];
    }

    const TT_ENTRY & operator[](const std::uint64_t zobrist_hash) const {
        return tt_table[zobrist_hash & mask];
    }
private:
    std::vector<TT_ENTRY> tt_table;
    std::uint64_t bucket_count;
    std::uint64_t mask; // mask == bucket_count - 1
};

#endif //MOTOR_TRANSPOSITION_TABLE_HPP
