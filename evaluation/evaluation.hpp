#ifndef MOTOR_EVALUATION_HPP
#define MOTOR_EVALUATION_HPP

#include "nnue.hpp"

constexpr bool SIDES[64] = {
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
};

template <Color color>
std::int16_t evaluate(const board& chessboard) {
    const bool wk = SIDES[chessboard.get_king<White>()];
    const bool bk = SIDES[chessboard.get_king<Black>()];
    return network.evaluate<color>(wk, bk);
}

#endif //MOTOR_EVALUATION_HPP