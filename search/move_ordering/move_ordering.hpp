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

constexpr int mvv[7] = { 500, 1000, 1000, 2000, 3000, 0, 1044 };

template <Color color>
void score_moves(board & chessboard, move_list & movelist, search_data & data, const chess_move & tt_move) {
    int move_index = 0;
    for (chess_move & move : movelist) {
        const Square from = move.get_from();
        const Square to   = move.get_to();
        int move_score;
        if (move == tt_move) {
            move_score = 214748364;
        } else if (!chessboard.is_quiet(move)) {
            int cap_score = history->get_capture_score<color>(chessboard, chessboard.get_piece(from), to, chessboard.get_piece(to));
            move_score = 10'000'000 * see<color>(chessboard, move, -cap_score / 40) + mvv[chessboard.get_piece(to)];
            move_score += cap_score;
        } else {
            move_score = history->get_quiet_score<color>(chessboard, data, from, to, chessboard.get_piece(from));
            move_score += 32'000 * (data.get_killer() == move);
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
