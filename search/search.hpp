#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include "time_keeper.hpp"
#include "../chess_board/board.hpp"

void find_best_move(board & chessboard, time_info & info) {
    time_keeper timekeeper;
    if(chessboard.get_side() == White) {
        timekeeper.reset(info.wtime, info.winc, info.movestogo);
    } else {
        timekeeper.reset(info.btime, info.binc, info.movestogo);
    }

}

#endif //MOTOR_SEARCH_HPP
