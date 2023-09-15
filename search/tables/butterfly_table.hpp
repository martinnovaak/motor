#ifndef MOTOR_BUTTERFLY_TABLE_HPP
#define MOTOR_BUTTERFLY_TABLE_HPP

#include <array>
#include <cstdint>
#include <cmath>

template <typename T>
class butterfly_table {
public:
    explicit butterfly_table(T initial_value) {
        this->initial_value = initial_value;
        clear();
    }

    void increase_value(std::uint8_t from, std::uint8_t to, T value) {
        table[from][to] += value;
    }

    void reduce_value(std::uint8_t from, std::uint8_t to, T value) {
        table[from][to] -= value;
    }

    void update_value(std::uint8_t from, std::uint8_t to, T value) {
        table[from][to] = value;
    }

    T get_value(std::uint8_t from, std::uint8_t to) {
        return table[from][to];
    }

    void clear() {
        std::fill_n(&table[0][0], 64 * 64, initial_value);
    }

    void centralize_whole_table() {
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                table[i][j] = std::abs(initial_value + table[i][j]) / 2;
            }
        }
    }
private:
    T initial_value;
    std::array<std::array<T, 64>, 64> table;
};

#endif //MOTOR_BUTTERFLY_TABLE_HPP
