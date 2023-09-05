#ifndef MOTOR_MOVE_ORDERING_HPP
#define MOTOR_MOVE_ORDERING_HPP

#include "../../move_generation/move_list.hpp"

void score_moves(move_list & movelist, const chess_move & tt_move) {
    for (chess_move & move : movelist) {
        if (move == tt_move) {
            move.set_score(16383);
        }
    }
}

#endif //MOTOR_MOVE_ORDERING_HPP
