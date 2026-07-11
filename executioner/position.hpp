#ifndef MOTOR_POSITION_HPP
#define MOTOR_POSITION_HPP

#include "../chess_board/board.hpp"
#include "../evaluation/nnue.hpp"

struct position {
    board chessboard;
    perspective_network<HIDDEN_SIZE> network;
};

#endif //MOTOR_POSITION_HPP
