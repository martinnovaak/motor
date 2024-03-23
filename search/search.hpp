#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include "search_data.hpp"
#include "tables/transposition_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "quiescence_search.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../evaluation/evaluation.hpp"

enum class NodeType : std::uint8_t {
    Root, PV, Non_PV, Null
};

enum class Bound : std::uint8_t {
    EXACT, // Type 1 - score is exact
    LOWER, // Type 2 - score is bigger than beta (fail-high) - Beta node
    UPPER  // Type 3 - score is lower than alpha (fail-low)  - Alpha node
};

struct TT_entry {
    Bound bound;            // 8 bits
    std::int8_t depth;      // 8 bits
    std::int16_t score;     // 16 bits
    chess_move tt_move;     // 32 bits
    std::uint64_t zobrist;  // 64 bits
};

transposition_table<TT_entry> tt(32 * 1024 * 1024);

template <Color color, NodeType node_type>
std::int16_t alpha_beta(board& chessboard, search_data& data, std::int16_t alpha, std::int16_t beta, std::int8_t depth) {
    constexpr Color enemy_color = color == White ? Black : White;
    constexpr bool is_pv = node_type == NodeType::PV || node_type == NodeType::Root;
    constexpr bool is_root = node_type == NodeType::Root;

    if (data.singular_move == 0 && data.should_end()) {
        return alpha;
    }

    data.update_pv_length();

    bool in_check = false;
    if constexpr (!is_root) {
        if (chessboard.is_draw()) {
            return 0;
        }

        in_check = chessboard.in_check();
        if (in_check) {
            depth++;
        }
    }

    if (depth <= 0) {
        return quiescence_search<color>(chessboard, data, alpha, beta);
    }

    Bound flag = Bound::UPPER;

    std::uint64_t zobrist_key = chessboard.get_hash_key();
    const TT_entry& tt_entry = tt[zobrist_key];

    chess_move best_move;
    chess_move tt_move = {};
    std::int16_t static_eval = evaluate<color>(chessboard);;
    std::int16_t eval = static_eval;
    bool tthit = false;

    if (data.singular_move == 0 && tt_entry.zobrist == zobrist_key) {
        best_move = tt_entry.tt_move;
        tt_move = tt_entry.tt_move;
        std::int16_t tt_eval = tt_entry.score;
        tthit = true;
        eval = tt_eval;
        static_eval = tt_eval;
        if constexpr (!is_pv) {
            if (tt_entry.depth >= depth) {
                if ((tt_entry.bound == Bound::EXACT) ||
                    (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
                    (tt_entry.bound == Bound::UPPER && tt_eval <= alpha)) {
                    return tt_eval;
                }
            }
        }
    }
    else {
        if (data.singular_move == 0 && depth >= 4) {
            depth--;
        }
    }

    data.improving[data.get_ply()] = static_eval;
    int improving = data.get_ply() > 1 && static_eval > data.improving[data.get_ply() - 2];

    data.prev_moves[data.get_ply()] = {};

    if constexpr (!is_root) {
        if (data.singular_move == 0 && !in_check && std::abs(beta) < 9'000) {
            // razoring
            if (depth < 7 && eval + 200 * depth <= alpha) {
                std::int16_t razor_eval = quiescence_search<color>(chessboard, data, alpha, beta);
                if (razor_eval <= alpha) {
                    return razor_eval;
                }
            }

            // reverse futility pruning
            if (depth < 7 && eval - 150 * depth / (1 + improving) >= beta) {
                return eval;
            }
            

            // NULL MOVE PRUNING
            if (node_type != NodeType::Null && depth >= 3 && eval >= beta && !chessboard.pawn_endgame()) {
                chessboard.make_null_move<color>();
                tt.prefetch(chessboard.get_hash_key());
                int R = 3 + depth / 3;
                data.augment_ply();
                std::int16_t nullmove_score = -alpha_beta<enemy_color, NodeType::Null>(chessboard, data, -beta, -alpha, depth - R);
                data.reduce_ply();
                chessboard.undo_null_move<color>();
                if (nullmove_score >= beta) {
                    return nullmove_score;
                }
            }
        }
    }

    move_list movelist, quiets, captures;
    generate_all_moves<color, GenType::ALL>(chessboard, movelist);

    if (movelist.size() == 0) {
        if (in_check) {
            return data.mate_value();
        } else {
            if (data.singular_move > 0) return alpha;
            return 0;
        }
    }

    std::int16_t best_score = -INF;
    score_moves<color>(chessboard, movelist, data, best_move);
    const chess_move previous_move = chessboard.get_last_played_move();

    double lmr_base = std::log2(depth) / 5.5;

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move& chessmove = movelist.get_next_move(moves_searched);

        if (chessmove.get_value() == data.singular_move) {
            continue;
        }

        int reduction = 1.0 + lmr_base * std::log2(moves_searched);

        if constexpr (!is_pv) {
            if (best_score > -9'000 && !in_check && movelist[moves_searched] < 15'000 && chessmove.get_check_type() == NOCHECK) {
                if (chessmove.is_quiet()) {
                    if (moves_searched > 4 + depth * depth) {
                        continue;
                    }
                }
            }
        }

        make_move<color>(chessboard, chessmove);
        tt.prefetch(chessboard.get_hash_key());
        data.augment_ply();

        std::int16_t score;
        if (moves_searched == 0) {
            score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1);
        }
        else {
            // late move reduction
            bool do_full_search = true;
            if (depth >= 3 && movelist[moves_searched] < 15'000 && chessmove.get_check_type() == NOCHECK && !in_check) {
                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - reduction - 1);
                do_full_search = score > alpha && reduction > 0;
            }

            if (do_full_search) {
                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - 1);
            }


            if (is_pv && score > alpha) {
                score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1);
            }
        }

        if (data.time_is_up()) {
            return 0;
        }

        undo_move<color>(chessboard);
        data.reduce_ply();

        if (score > best_score) {
            best_score = score;
            best_move = chessmove;
            data.update_pv(chessmove);

            if (score > alpha) {
                alpha = score;
                flag = Bound::EXACT;

                if (alpha >= beta) {
                    flag = Bound::LOWER;
                    if (chessmove.is_quiet()) {
                        data.update_killer(chessmove);
                        data.update_history(chessmove.get_from(), chessmove.get_to(), depth);
                        data.counter_moves[previous_move.get_from()][previous_move.get_to()] = chessmove;   

                        for (const auto& quiet : quiets) {
                            data.reduce_history(quiet.get_from(), quiet.get_to(), depth);
                        }
                    }
                    break;
                }
            }
        }

        if (chessmove.is_quiet()) {
            quiets.add(chessmove);
        }
        else {
            captures.add(chessmove);
        }
    }

    if (data.singular_move == 0)
        tt[zobrist_key] = { flag, depth, best_score, best_move, zobrist_key };

    return best_score;
}

template <Color color>
std::int16_t aspiration_window(board& chessboard, search_data& data, std::int16_t score, int depth) {
    std::int16_t window = 20;
    std::int16_t alpha, beta;

    int search_depth = depth;

    alpha = std::max(static_cast<std::int16_t>(-INF), static_cast<std::int16_t>(score - window));
    beta = std::min(INF, static_cast<std::int16_t>(score + window));

    while (!data.time_is_up()) {


        score = alpha_beta<color, NodeType::Root>(chessboard, data, alpha, beta, search_depth);
        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(static_cast<std::int16_t>(-INF), static_cast<std::int16_t>(alpha - window));
            search_depth = depth;
        }
        else if (score >= beta) {
            beta = std::min(INF, static_cast<std::int16_t>(beta + window));
            search_depth--;
        }
        else {
            break;
        }

        window += window / 2;
        if (window > 500) {
            window = INF;
        }
    }

    return score;
}

template <Color color>
void iterative_deepening(board& chessboard, search_data& data, int max_depth) {
    std::string best_move;
    int score;
    for (int depth = 1; depth <= max_depth; depth++) {
        if (depth < 6) {
            score = alpha_beta<color, NodeType::Root>(chessboard, data, -10'000, 10'000, depth);
        }
        else {
            score = aspiration_window<color>(chessboard, data, score, depth);
        }

        if (data.time_is_up()) {
            break;
        }
        std::cout << "info depth " << depth << " score cp " << score << " nodes " << data.nodes() << " nps " << data.nps() << " pv " << data.get_pv(depth) << std::endl;
        data.reset_nodes();

        best_move = data.get_best_move();

        if (std::abs(score) > 8'000) {
            break;
        }
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board& chessboard, time_info& info) {
    search_data data;
    history_table.centralize_whole_table();
    if (chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo);
        iterative_deepening<White>(chessboard, data, info.max_depth);
    }
    else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo);
        iterative_deepening<Black>(chessboard, data, info.max_depth);
    }
}

#endif //MOTOR_SEARCH_HPP
