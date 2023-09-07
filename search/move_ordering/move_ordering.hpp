#ifndef MOTOR_MOVE_ORDERING_HPP
#define MOTOR_MOVE_ORDERING_HPP

#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"

constexpr static int mvv_lva[7][6] = {
//attacker:  P, N,  B,  R,  Q,  NONE
        {6,  5, 4, 3, 2, 1},    // victim PAWN
        {12, 11, 10, 9, 8, 7, },   // victim KNIGHT
        {18, 17, 16, 15, 14, 13},    // victim BISHOP
        {24, 23, 22, 21, 20, 19},    // victim ROOK
        {31, 30, 29, 28, 27, 26},    // victim QUEEN
        {0,  0,  0,  0,  0,  0},    // victim NONE
        {25, 0, 0, 0, 0, 0}
};

void score_moves(board & chessboard, move_list & movelist, search_data & data, const chess_move & tt_move) {
    for (chess_move & move : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        if (move == tt_move) {
            move.set_score(16383);
        } else if (!move.is_quiet()) {
            move.set_score(15000 + mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)]);
        } else if (data.get_killer(0) == move){
            move.set_score(14999);
        } else if (data.get_killer(1) == move){
            move.set_score(14998);
        } else {
            move.set_score(data.get_history(from, to));
        }
    }
}

#endif //MOTOR_MOVE_ORDERING_HPP
