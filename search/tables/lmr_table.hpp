#ifndef MOTOR_LMR_TABLE_HPP
#define MOTOR_LMR_TABLE_HPP

#include <array>
#include <cmath>

int computeReductionValue(int i, int j) {
    return static_cast<int>(1.0 + std::log2(static_cast<double>(i)) * std::log2(static_cast<double>(j)) / 5.2);
}

std::array<std::array<int, 218>, 96> initializeReductions() {
    std::array<std::array<int, 218>, 96> reductions = {};

    reductions[0][0] = 0;

    for (int i = 1; i < 96; ++i) {
        for (int j = 1; j < 218; ++j) {
            reductions[i][j] = computeReductionValue(i, j);
        }
    }

    return reductions;
}

auto lmr_table = initializeReductions();

#endif //MOTOR_LMR_TABLE_HPP
