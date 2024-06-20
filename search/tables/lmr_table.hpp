#ifndef MOTOR_LMR_TABLE_HPP
#define MOTOR_LMR_TABLE_HPP

#include "../chess_board/types.hpp"
#include <cmath>

nested_array<int, 96, 220> initializeReductions(int lmr = 550) {
    nested_array<int, 96, 220> reductions = {};

    for (int i = 1; i < 96; i++) {
        for (int j = 1; j < 218; j++) {
            reductions[i][j] = static_cast<int>(1.0 + std::log2(static_cast<double>(i)) * std::log2(static_cast<double>(j)) * 100 / lmr);
        }
    }

    return reductions;
}

auto lmr_table = initializeReductions();

#endif //MOTOR_LMR_TABLE_HPP
