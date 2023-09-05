#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include "search_data.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../evaluation/evaluation.hpp"

template <Color color>
std::int16_t alpha_beta(board & chessboard, search_data & data, std::int16_t alpha, std::int16_t beta, std::int8_t depth) {
    constexpr Color enemy_color = color == White ? Black : White;

    if(data.should_end()) {
        return alpha;
    }

    data.update_pv_length();

    if (chessboard.is_draw()) {
        return 0;
    }

    if (depth <= 0) {
        return evaluate<color>(chessboard);
    }

    std::int16_t best_score = -INF;
    move_list movelist;
    generate_all_moves<color, false>(chessboard, movelist);

    if (movelist.size() == 0) {
        if (chessboard.in_check()) {
            return data.mate_value();
        } else {
            return 0;
        }
    }

    for (auto move : movelist) {
        chessboard.make_move<color>(move);
        data.augment_ply();

        std::int16_t score = -alpha_beta<enemy_color>(chessboard, data, -beta, -alpha, depth - 1);

        chessboard.undo_move<color>();
        data.reduce_ply();

        if (score > best_score) {
            best_score = score;
            data.update_pv(move);
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta) {
            break;
        }
    }
    return best_score;
}

void find_best_move(board & chessboard, time_info & info) {
    search_data data;
    int score;
    if(chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo);
        score = alpha_beta<White>(chessboard, data, -10'000, 10'000, 5);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo);
        score = alpha_beta<Black>(chessboard, data, -10'000, 10'000, 5);
    }

    std::cout << " info score cp " << score << "\n";
    std::cout << "bestmove " << data.get_best_move() << "\n";
}

#endif //MOTOR_SEARCH_HPP
