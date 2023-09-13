#ifndef MOTOR_MOVE_ORDERING_HPP
#define MOTOR_MOVE_ORDERING_HPP

#include "see.hpp"
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

template <Color color>
void score_moves(board & chessboard, move_list & movelist, search_data & data, const chess_move & tt_move) {
    for (chess_move & move : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        const chess_move previous_move = chessboard.get_last_played_move();
        const chess_move counter_move = data.counter_moves[previous_move.get_from()][previous_move.get_to()];
        if (move == tt_move) {
            move.set_score(16383);
        } else if (!move.is_quiet()) {
            move.set_score(15003 * see<color>(chessboard, move) + mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)]);
        } else if (data.get_killer(0) == move){
            move.set_score(15002);
        } else if (data.get_killer(1) == move){
            move.set_score(15001);
        } else if (counter_move == move) {
            move.set_score(15'000);
        } else {
            move.set_score(data.get_history(from, to));
        }
    }
}

static void qs_score_moves(board & chessboard, move_list & movelist) {
    for(chess_move & move : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        move.set_score(mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)]);
    }
}

#endif //MOTOR_MOVE_ORDERING_HPP
