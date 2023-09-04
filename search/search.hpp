#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include "search_data.hpp"
#include "../chess_board/board.hpp"

void find_best_move(board & chessboard, time_info & info) {
    search_data data;
    if(chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo);
    }
}

#endif //MOTOR_SEARCH_HPP
