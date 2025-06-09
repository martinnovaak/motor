#ifndef MOTOR_QUIESCENCE_SEARCH_HPP
#define MOTOR_QUIESCENCE_SEARCH_HPP

#include "search_data.hpp"
#include "tables/transposition_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../executioner/makemove.hpp"

template <Color color, NodeType node_type>
std::int16_t quiescence_search(board & chessboard, search_data & data, std::int16_t alpha, std::int16_t beta, std::int8_t depth = 0) {
    constexpr Color enemy_color = (color == White) ? Black : White;

    if(data.should_end()) {
        return beta;
    }

    if (data.get_ply() > 92) {
        return evaluate<color>(chessboard);
    }

    if (chessboard.is_draw(data.get_ply())) {
        return 0;
    }

    bool in_check = chessboard.in_check();
    std::int16_t static_eval, eval;

    Bound flag = Bound::UPPER;

    std::uint64_t zobrist_key = chessboard.get_hash_key();
    const TT_entry& tt_entry = tt.retrieve(zobrist_key, data.get_ply());
    chess_move tt_move = {};

    if (tt_entry.zobrist == tt.upper(zobrist_key)) {
        std::int16_t tt_eval = tt_entry.score;
        tt_move = tt_entry.tt_move;
        static_eval = tt_entry.static_eval;
        eval = history->correct_eval<color>(chessboard, data, static_eval);
        if ((node_type == NodeType::Non_PV) && ((tt_entry.bound == Bound::EXACT) ||
            (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
            (tt_entry.bound == Bound::UPPER && tt_eval <= alpha))) {
            return tt_eval;
        }

        if (!((eval > tt_eval && tt_entry.bound == Bound::LOWER) || (eval < tt_eval && tt_entry.bound == Bound::UPPER)))
        {
            eval = tt_eval;
        }
    } else {
        static_eval = eval = in_check ? -INF : evaluate<color>(chessboard);
        eval = history->correct_eval<color>(chessboard, data,static_eval);
    }

    if (eval >= beta) {
        return eval;
    }

    if (eval > alpha) {
        alpha = eval;
    }

    move_list movelist;
    if (in_check) {
        generate_all_moves<color, false>(chessboard, movelist);
        score_moves<color>(chessboard, movelist, data, tt_move);
        if (movelist.size() == 0) {
            return data.mate_value();
        }
    } else {
        generate_all_moves<color, true>(chessboard, movelist);
        qs_score_moves(chessboard, movelist);
    }

    std::int16_t futility_base = eval + 250;

    chess_move best_move;

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move & chessmove = movelist.get_next_move(moves_searched);

        if (in_check && movelist.get_move_score(moves_searched) < 16'000 && moves_searched) {
            break;
        }

        if (!in_check) {
            if (chessboard.is_capture(chessmove) && futility_base <= alpha && !see<color>(chessboard, chessmove, 1)) {
                eval = std::max(eval, futility_base);
                continue;
            }

            if (!see<color>(chessboard, chessmove)) {
                continue;
            }
        }

        make_move<color>(chessboard, chessmove);
        data.augment_ply();
        tt.prefetch(chessboard.get_hash_key());
        std::int16_t score = -quiescence_search<enemy_color, node_type>(chessboard, data, -beta, -alpha, depth - 1);
        undo_move<color>(chessboard, chessmove);
        data.reduce_ply();

        if (score <= eval) {
            continue;
        }

        eval = score;

        if (beta <= eval) {
            best_move = chessmove;
            flag = Bound::LOWER;
            break;
        }

        if (alpha <= eval) {
            alpha = eval;
            flag = Bound::EXACT;
            best_move = chessmove;
        }
    }

    tt.store(flag, 0, eval, static_eval, best_move, data.get_ply(), false, zobrist_key);
    return eval;
}

#endif //MOTOR_QUIESCENCE_SEARCH_HPP