#ifndef MOTOR_LMR_TABLE_HPP
#define MOTOR_LMR_TABLE_HPP

#include <array>
#include <cmath>
#include <algorithm>

std::array<std::array<int, 218>, 96> initializeReductions(int lmr = 420) {
    std::array<std::array<int, 218>, 96> reductions = {};

    for (int i = 1; i < 96; i++) {
        for (int j = 1; j < 218; j++) {
            reductions[i][j] = static_cast<int>(1.0 + std::log2(static_cast<double>(i)) * std::log2(static_cast<double>(j)) * 100 / lmr);
        }
    }

    return reductions;
}

std::array<int, 256> initializeCorrhistBonus() {
    std::array<int, 256> bonus_table = {};

    for (int j = 1; j <= 256; j++) {
        double logBase32 = std::log(j + 1) / std::log(32);
        double sinValue = std::sin(logBase32);
        double powerValue = std::pow(sinValue, 1.75);
        double product = powerValue * 256;
        bonus_table[j - 1] = static_cast<int>(std::min(product, 300.0));
    }

    return bonus_table;
}

auto lmr_table = initializeReductions();
auto corrhist_bonus_table = initializeCorrhistBonus();

#endif //MOTOR_LMR_TABLE_HPP
