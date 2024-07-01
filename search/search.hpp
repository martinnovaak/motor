#ifndef MOTOR_SEARCH_HPP
#define MOTOR_SEARCH_HPP

#include <iostream>

#include "search_data.hpp"
#include "tables/transposition_table.hpp"
#include "tables/lmr_table.hpp"
#include "tables/history_table.hpp"
#include "move_ordering/move_ordering.hpp"
#include "quiescence_search.hpp"
#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../executioner/makemove.hpp"

constexpr int iir_depth = 3;
constexpr int razoring = 457;
constexpr int razoring_depth = 4;
constexpr int rfp = 154;
constexpr int rfp_depth = 9;
constexpr int nmp = 3;
constexpr int nmp_div = 4;
constexpr int nmp_depth = 2;
constexpr int lmp_base = 2;
constexpr int fp_base = 129;
constexpr int fp_mul = 286;
constexpr int fp_depth = 7;
constexpr int see_quiet = 93;
constexpr int see_noisy = 39;
constexpr int see_depth = 6;

constexpr int se_depth = 7;
constexpr int se_depth_margin = 2;
constexpr int se_mul = 114;
constexpr int double_margin = 20;
constexpr int double_exts = 4;

constexpr int lmr_depth = 2;
constexpr int lmr_quiet_history = 12600;
constexpr int asp_window = 19;
constexpr int asp_window_mul = 15;
constexpr int asp_window_max = 666;
constexpr int asp_depth = 8;

template <Color color, NodeType node_type>
std::int16_t alpha_beta(board& chessboard, search_data& data, std::int16_t alpha, std::int16_t beta, std::int8_t depth) {
    constexpr Color enemy_color = color == White ? Black : White;
    constexpr bool is_pv = node_type == NodeType::PV || node_type == NodeType::Root;
    constexpr bool is_root = node_type == NodeType::Root;

    if (!(is_root && depth < 3) && data.should_end()) {
        return beta;
    }

    if (data.get_ply() > 92) {
        return evaluate<color>(chessboard);
    }

    data.update_pv_length();

    bool in_check = false;
    if constexpr (!is_root) {
        if (chessboard.is_draw()) {
            return 0;
        }

        // mate distance pruning
        alpha = std::max(data.mate_value(), alpha);
        beta = std::min(static_cast<std::int16_t>(-data.mate_value() - 1), beta);
        if (alpha >= beta) {
            return alpha;
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
    std::int16_t eval, static_eval;

    if (data.singular_move == 0 && tt_entry.zobrist == zobrist_key) {
        best_move = tt_entry.tt_move;
        tt_move = tt_entry.tt_move;
        std::int16_t tt_eval = tt_entry.score;
        eval = static_eval = tt_entry.static_eval;

        if constexpr (!is_pv) {
            if (tt_entry.depth >= depth) {
                if ((tt_entry.bound == Bound::EXACT) ||
                    (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
                    (tt_entry.bound == Bound::UPPER && tt_eval <= alpha)) {
                    return tt_eval;
                }
            }
        }

        if (!((eval > tt_eval && tt_entry.bound == Bound::LOWER) || (eval < tt_eval && tt_entry.bound == Bound::UPPER)))
        {
            eval = tt_eval;
        }
    } else {
        eval = static_eval = evaluate<color>(chessboard);
        if (data.singular_move == 0 && depth >= iir_depth) {
            depth--;
        }
    }

    data.improving[data.get_ply()] = static_eval;
    int improving = data.get_ply() > 1 && static_eval > data.improving[data.get_ply() - 2];

    data.prev_moves[data.get_ply()] = {};
    data.reset_killers();

    if constexpr (!is_root) {
        if (!in_check && std::abs(beta) < 9'000) {
            // razoring
            if (depth < razoring_depth && eval + razoring * depth <= alpha) {
                std::int16_t razor_eval = quiescence_search<color>(chessboard, data, alpha, beta);
                if (razor_eval <= alpha) {
                    return razor_eval;
                }
            }

            // reverse futility pruning
            if (depth < rfp_depth && eval - rfp * (depth - improving) >= beta) {
                return eval;
            }

            // NULL MOVE PRUNING
            if (node_type != NodeType::Null && depth >= nmp_depth && eval >= beta && !chessboard.pawn_endgame()) {
                chessboard.make_null_move<color>();
                tt.prefetch(chessboard.get_hash_key());
                int R = nmp + depth / nmp_div + improving;
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

    if constexpr (!is_root) {
        data.double_extension[data.get_ply()] = data.double_extension[data.get_ply() - 1];
    }

    move_list movelist, quiets, captures;
    generate_all_moves<color, false>(chessboard, movelist);

    if (movelist.size() == 0) {
        if (data.singular_move > 0) return alpha;
        if (in_check) {
            return data.mate_value();
        } else {
            return 0;
        }
    }

    std::int16_t best_score = -INF;
    score_moves<color>(chessboard, movelist, data, best_move);
    const chess_move previous_move = chessboard.get_last_played_move();

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move& chessmove = movelist.get_next_move(moves_searched);

        if (chessmove.get_value() == data.singular_move) {
            continue;
        }

        std::uint64_t start_nodes = data.get_nodes();

        int reduction = lmr_table[depth][moves_searched];
        bool is_quiet = chessboard.is_quiet(chessmove);

        if constexpr (!is_root) {
            if (moves_searched && best_score > -9'000 && !in_check && movelist[moves_searched] < 15'000) {
                if (is_quiet) {
                    if (quiets.size() > lmp_base + depth * depth / (2 - improving)) {
                        continue;
                    }

                    int lmr_depth = std::max(0, depth - reduction);
                    if (lmr_depth < fp_depth && static_eval + fp_base + fp_mul * lmr_depth <= alpha) {
                        continue;
                    }
                }


                int see_margin = is_quiet ? -see_quiet * depth : -see_noisy * depth * depth;
                if (depth < see_depth && !see<color>(chessboard, chessmove, see_margin)) {
                    continue;
                }
            }
        }

        int ext = 0;

        if constexpr (!is_root) {
            if (depth >= se_depth &&
                moves_searched == 0 &&
                movelist.get_move_score(moves_searched) == 214'748'364 &&
                tt_entry.depth >= depth - se_depth_margin &&
                tt_entry.bound != Bound::UPPER &&
                data.singular_move == 0 &&
                std::abs(tt_entry.score) < 9'000)
            {
                int s_beta = tt_entry.score - se_mul * depth / 80;
                data.singular_move = chessmove.get_value();
                int s_score = alpha_beta<color, NodeType::Non_PV>(chessboard, data, s_beta - 1, s_beta, (depth - 1) / 2);
                data.singular_move = 0;
                if (s_score < s_beta) {
                    ext = 1;
                    if constexpr(!is_pv) {
                        if (s_score + double_margin < s_beta && data.double_extension[data.get_ply()] < double_exts) {
                            ext = 2;
                            data.double_extension[data.get_ply()]++;
                        }
                    }
                }
                else if (s_beta >= beta) {
                    return s_beta;
                }
            }
        }

        auto from = chessmove.get_from();
        auto to = chessmove.get_to();
        auto piece = chessboard.get_piece(from);
        data.prev_moves[data.get_ply()] = { piece, from, to };
        make_move<color, true>(chessboard, chessmove);
        tt.prefetch(chessboard.get_hash_key());
        data.augment_ply();

        std::int16_t score;
        if (moves_searched == 0) {
            score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1 + ext);
        }
        else {
            // late move reduction
            bool do_full_search = true;
            if (depth >= lmr_depth && movelist.get_move_score(moves_searched) < 1'000'000) {
                if (is_quiet) {
                    reduction += !is_pv + !improving;
                    reduction -= chessboard.in_check();
                    reduction -= movelist.get_move_score(moves_searched) / lmr_quiet_history;
                }

                reduction = std::clamp(reduction, 0, depth - 2);

                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - reduction - 1 + ext);
                do_full_search = score > alpha && reduction > 0;
            }

            if (do_full_search) {
                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - 1 + ext);
            }


            if (is_pv && score > alpha) {
                score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1 + ext);
            }
        }

        undo_move<color>(chessboard, chessmove);
        data.reduce_ply();

        if constexpr (is_root) {
            data.update_node_count(from, to, start_nodes);
        }

        if (score > best_score) {
            best_score = score;
            best_move = chessmove;
            data.update_pv(chessmove);
            if constexpr (is_root) {
                data.best_move = chessmove.to_string();
            }

            if (score > alpha) {
                alpha = score;
                flag = Bound::EXACT;

                if (alpha >= beta) {
                    flag = Bound::LOWER;
                    int bonus = history_bonus(depth);
                    if (is_quiet) {
                        data.update_killer(chessmove);
                        data.counter_moves[previous_move.get_from()][previous_move.get_to()] = chessmove;
                    }
                    update_history<color, is_root>(data, chessboard, best_move, quiets, captures, depth);
                    break;
                }
            }
        }

        if (is_quiet) {
            quiets.push_back(chessmove);
        } else {
            captures.push_back(chessmove);
        }
    }

    if (data.singular_move == 0)
        tt[zobrist_key] = { flag, depth, best_score, static_eval, best_move, zobrist_key };

    return best_score;
}

template <Color color>
std::int16_t aspiration_window(board& chessboard, search_data& data, std::int16_t score, int depth) {
    std::int16_t window = asp_window;
    std::int16_t alpha, beta;

    int search_depth = depth;

    alpha = std::max(static_cast<std::int16_t>(-INF), static_cast<std::int16_t>(score - window));
    beta = std::min(INF, static_cast<std::int16_t>(score + window));

    while (!data.time_stopped()) {
        score = alpha_beta<color, NodeType::Root>(chessboard, data, alpha, beta, search_depth);
        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(static_cast<std::int16_t>(-INF), static_cast<std::int16_t>(alpha - window));
            search_depth = depth;
        }
        else if (score >= beta) {
            beta = std::min(INF, static_cast<std::int16_t>(beta + window));
            search_depth = std::max(1, search_depth - 1);
        }
        else {
            break;
        }

        window += window * asp_window_mul / 32;
        if (window > asp_window_max) {
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
        if (depth > 1 && data.time_is_up(depth)) {
            break;
        }

        if (depth < asp_depth) {
            score = alpha_beta<color, NodeType::Root>(chessboard, data, -10'000, 10'000, depth);
        } else {
            score = aspiration_window<color>(chessboard, data, score, depth);
        }

        if (depth > 1 && data.time_stopped()) {
            break;
        }

        std::cout << "info depth " << depth << " score cp " << score << " nodes " << data.nodes() << " nps " << data.nps() << " pv " << data.get_pv(depth) << std::endl;
        data.reset_nodes();

        best_move = data.best_move;

        if (depth > 10 && std::abs(score) > 19'000) {
            break;
        }
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board& chessboard, time_info& info) {
    search_data data;

    if (chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo, chessboard.move_count());
        iterative_deepening<White>(chessboard, data, info.max_depth);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo, chessboard.move_count());
        iterative_deepening<Black>(chessboard, data, info.max_depth);
    }
}

#endif //MOTOR_SEARCH_HPP
