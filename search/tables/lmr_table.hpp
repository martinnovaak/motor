#ifndef MOTOR_LMR_TABLE_HPP
#define MOTOR_LMR_TABLE_HPP

#include <array>
#include <cmath>

std::array<std::array<int, 218>, 96> initializeReductions(int lmr = 420) {
    std::array<std::array<int, 218>, 96> reductions = {};

    for (int i = 1; i < 96; i++) {
        for (int j = 1; j < 218; j++) {
            reductions[i][j] = static_cast<int>(1.0 + std::log2(static_cast<double>(i)) * std::log2(static_cast<double>(j)) * 100 / lmr);
        }
    }

    return reductions;
}

auto lmr_table = initializeReductions();

std::array<int, 256> initializeCorrhistBonus() {
    std::array<int, 256> bonus_table = {};

    for (int j = 1; j <= 256; j++) {
        bonus_table[j - 1] = std::log2(std::sinh(2 * j * j));
    }

    return bonus_table;
}

auto corrhist_bonus_table = initializeCorrhistBonus();

#endif //MOTOR_LMR_TABLE_HPP
