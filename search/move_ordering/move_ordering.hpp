#ifndef MOTOR_MOVE_ORDERING_HPP
#define MOTOR_MOVE_ORDERING_HPP

#include "see.hpp"
#include "../../generator/movelist.hpp"
#include "../search_data.hpp"

constexpr static int mvv_lva[7][6] = {
//attacker:  P, N,  B,  R,  Q,  NONE
        {6,  5, 4, 3, 2, 1},        // victim PAWN
        {12, 11, 10, 9, 8, 7, },    // victim KNIGHT
        {18, 17, 16, 15, 14, 13},   // victim BISHOP
        {24, 23, 22, 21, 20, 19},   // victim ROOK
        {31, 30, 29, 28, 27, 26},   // victim QUEEN
        {0,  0,  0,  0,  0,  0},    // victim NONE
        {25, 0, 0, 0, 0, 0}
};

template <Color color>
void score_moves(board & chessboard, movelist & movelist, search_data & data, const chessmove & tt_move) {
    for (auto & [move, score] : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        const chessmove previous_move = chessboard.get_last_played_move();
        const chessmove counter_move = data.counter_moves[previous_move.get_from()][previous_move.get_to()];
        if (move == tt_move) {
            score = 214748364;
        } else if (!move.is_quiet()) {
            score = 10'000'000 * see<color>(chessboard, move) + mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)];
        } else if (data.get_killer(0) == move){
            score = 1'000'002;
        } else if (data.get_killer(1) == move){
            score = 1'000'001;
        } else if (counter_move == move) {
            score = 1'000'000;
        } else {
            score = data.get_history(from, to);
        }
    }
}

static void qs_score_moves(board & chessboard, movelist & movelist) {
    for(auto & [move, score] : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        score = mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)];
    }
}

#endif //MOTOR_MOVE_ORDERING_HPP
