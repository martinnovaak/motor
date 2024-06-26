#ifndef MOTOR_QUIESCENCE_SEARCH_HPP
#define MOTOR_QUIESCENCE_SEARCH_HPP

#include "search_data.hpp"
#include "tables/transposition_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../executioner/makemove.hpp"

template <Color color>
std::int16_t quiescence_search(board & chessboard, search_data & data, std::int16_t alpha, std::int16_t beta) {
    constexpr Color enemy_color = (color == White) ? Black : White;

    if(data.should_end()) {
        return beta;
    }

    std::int16_t static_eval, eval;

    Bound flag = Bound::UPPER;

    std::uint64_t zobrist_key = chessboard.get_hash_key();
    const TT_entry& tt_entry = tt[zobrist_key];

    if (tt_entry.zobrist == zobrist_key) {
        std::int16_t tt_eval = tt_entry.score;
        static_eval = tt_entry.static_eval;
        eval = tt_eval;
        if ((tt_entry.bound == Bound::EXACT) ||
            (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
            (tt_entry.bound == Bound::UPPER && tt_eval <= alpha)) {
            return tt_eval;
        }
    } else {
        static_eval = eval = evaluate<color>(chessboard);
    }

    if (eval >= beta) {
        return eval;
    }

    if (eval > alpha) {
        alpha = eval;
    }

    move_list movelist;
    generate_all_moves<color, true>(chessboard, movelist);
    qs_score_moves(chessboard, movelist);

    chess_move best_move;

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move & chessmove = movelist.get_next_move(moves_searched);

        if (!see<color>(chessboard, chessmove)) {
            continue;
        }

        make_move<color>(chessboard, chessmove);
        tt.prefetch(chessboard.get_hash_key());
        std::int16_t score = -quiescence_search<enemy_color>(chessboard, data, -beta, -alpha);
        undo_move<color>(chessboard, chessmove);

        if (score <= eval) {
            continue;
        }

        eval = score;
        best_move = chessmove;

        if (beta <= eval) {
            flag = Bound::LOWER;
            break;
        }

        if (alpha <= eval) {
            alpha = eval;
            flag = Bound::EXACT;
        }
    }

    tt[zobrist_key] = { flag, 0, eval, static_eval, best_move, zobrist_key };
    return eval;
}

#endif //MOTOR_QUIESCENCE_SEARCH_HPP
