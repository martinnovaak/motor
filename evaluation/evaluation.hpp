#ifndef MOTOR_EVALUATION_HPP
#define MOTOR_EVALUATION_HPP

#include "nnue.hpp"

template <Color color>
std::int16_t evaluate(board & chessboard) {
    return nn_eval<color>(chessboard);
}

#endif //MOTOR_EVALUATION_HPP
