#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include "search_data.hpp"
#include "transposition_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../evaluation/evaluation.hpp"

enum class Bound : std::uint8_t {
    EXACT, // Type 1 - score is exact
    LOWER, // Type 2 - score is bigger than beta (fail-high) - Beta node
    UPPER  // Type 3 - score is lower than alpha (fail-low)  - Alpha node
};

struct TT_entry {
    Bound bound;            // 8 bits
    std::int8_t depth;     // 8 bits
    std::int16_t score;     // 16 bits
    chess_move tt_move;     // 32 bits
    std::uint64_t zobrist;  // 64 bits
};

transposition_table<TT_entry> tt(32 * 1024 * 1024);

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

    Bound flag = Bound::UPPER;

    std::uint64_t zobrist_key = chessboard.get_hash_key();
    const TT_entry & tt_entry = tt[zobrist_key];
    chess_move best_move;
    if (tt_entry.zobrist == zobrist_key) {
        best_move = tt_entry.tt_move;
    }

    move_list movelist;
    generate_all_moves<color, false>(chessboard, movelist);

    if (movelist.size() == 0) {
        if (chessboard.in_check()) {
            return data.mate_value();
        } else {
            return 0;
        }
    }

    std::int16_t best_score = -INF;
    score_moves(movelist, best_move);

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        const chess_move & chessmove = movelist.get_next_move(moves_searched);
        chessboard.make_move<color>(chessmove);
        data.augment_ply();

        std::int16_t score = -alpha_beta<enemy_color>(chessboard, data, -beta, -alpha, depth - 1);

        chessboard.undo_move<color>();
        data.reduce_ply();

        if (score > best_score) {
            best_score = score;
            data.update_pv(chessmove);
        }

        if (score > alpha) {
            alpha = score;
            flag = Bound::EXACT;
        }

        if (alpha >= beta) {
            flag = Bound::LOWER;
            break;
        }
    }

    tt[zobrist_key] = {flag, depth, best_score, best_move, zobrist_key};
    return best_score;
}

template <Color color>
void iterative_deepening(board & chessboard, search_data & data) {
    std::string best_move;
    int score;
    for (int depth = 1; depth < MAX_DEPTH; depth++) {
        score = alpha_beta<color>(chessboard, data, -10'000, 10'000, depth);

        if (data.time_is_up()) {
            break;
        }

        best_move = data.get_best_move();
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board & chessboard, time_info & info) {
    search_data data;
    if(chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo);
        iterative_deepening<White>(chessboard, data);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo);
        iterative_deepening<Black>(chessboard, data);
    }
}

#endif //MOTOR_SEARCH_HPP
