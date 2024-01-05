#ifndef MOTOR_QUIESCENCE_SEARCH_HPP
#define MOTOR_QUIESCENCE_SEARCH_HPP

#include "search_data.hpp"
#include "tables/transposition_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../evaluation/evaluation.hpp"

template <Color color>
std::int16_t quiescence_search(board & chessboard, search_data & data, std::int16_t alpha, std::int16_t beta) {
    constexpr Color enemy_color = (color == White) ? Black : White;

    if(data.should_end()) {
        return 0;
    }

    std::int16_t eval = evaluate<color>(chessboard);

    if (eval >= beta) {
        return beta;
    }

    if (eval > alpha) {
        alpha = eval;
    }

    move_list movelist;
    generate_all_moves<color, true>(chessboard, movelist);
    qs_score_moves(chessboard, movelist);

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move & chessmove = movelist.get_next_move(moves_searched);

        if (!see<color>(chessboard, chessmove)) {
            continue;
        }

        make_move<color>(chessboard, chessmove);
        std::int16_t score = -quiescence_search<enemy_color>(chessboard, data, -beta, -alpha);
        undo_move<color>(chessboard);

        eval = std::max(eval, score);
        if (eval >= beta) {
            break;
        }
        alpha = std::max(eval, alpha);
    }

    return eval;
}

#endif //MOTOR_QUIESCENCE_SEARCH_HPP
