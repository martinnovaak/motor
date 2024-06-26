#ifndef MOTOR_LMR_TABLE_HPP
#define MOTOR_LMR_TABLE_HPP

#include <array>
#include <cmath>

std::array<std::array<int, 218>, 96> initializeReductions(int lmr = 515) {
    std::array<std::array<int, 218>, 96> reductions = {};

    for (int i = 1; i < 96; i++) {
        for (int j = 1; j < 218; j++) {
            reductions[i][j] = static_cast<int>(1.0 + std::log2(static_cast<double>(i)) * std::log2(static_cast<double>(j)) * 100 / lmr);
        }
    }

    return reductions;
}

auto lmr_table = initializeReductions();

#endif //MOTOR_LMR_TABLE_HPP
