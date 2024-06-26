#ifndef MOTOR_MOVE_ORDERING_HPP
#define MOTOR_MOVE_ORDERING_HPP

#include "see.hpp"
#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"
#include "../tables/history_table.hpp"

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

constexpr int noisy_base = -409;

constexpr int mvv[7] = { 147, 441, 312, 1039, 1232, 0, 1044 };

template <Color color>
void score_moves(board & chessboard, move_list & movelist, search_data & data, const chess_move & tt_move) {
    int move_index = 0;
    for (chess_move & move : movelist) {
        const Square from = move.get_from();
        const Square to   = move.get_to();
        const chess_move previous_move = chessboard.get_last_played_move();
        const chess_move counter_move = data.counter_moves[previous_move.get_from()][previous_move.get_to()];
        int move_score;
        if (move == tt_move) {
            move_score = 214748364;
        } else if (!chessboard.is_quiet(move)) {
            move_score = 10'000'000 * see<color>(chessboard, move) + mvv[chessboard.get_piece(to)];
            move_score += capture_table[chessboard.get_piece(from)][to][chessboard.get_piece(to)];
        } else if (data.get_killer(0) == move){
            move_score = 1'000'002;
        } else if (data.get_killer(1) == move){
            move_score = 1'000'001;
        } else if (move == counter_move) {
            move_score = 1'000'000;
        } else {
            move_score = get_history<color>(chessboard, data, from, to, chessboard.get_piece(from));
        }
        
        movelist[move_index] = move_score;
        move_index++;
    }
}

void qs_score_moves(board & chessboard, move_list & movelist) {
    int move_index = 0;
    for(chess_move & move : movelist) {
        const std::uint8_t from = move.get_from();
        const std::uint8_t to   = move.get_to();
        movelist[move_index] = mvv_lva[chessboard.get_piece(to)][chessboard.get_piece(from)];
        move_index++;
    }
}

#endif //MOTOR_MOVE_ORDERING_HPP
