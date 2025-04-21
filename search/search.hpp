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

constexpr int iir_depth = 4;
constexpr int razoring = 500;
constexpr int razoring_depth = 3;
constexpr int rfp = 154;
constexpr int rfp_depth = 9;
constexpr int nmp = 3;
constexpr int nmp_div = 3;
constexpr int nmp_depth = 3;
constexpr int lmp_base = 2;
constexpr int fp_base = 124;
constexpr int fp_mul = 305;
constexpr int fp_depth = 7;
constexpr int see_quiet = 97;
constexpr int see_noisy = 36;
constexpr int see_depth = 6;

constexpr int se_depth = 6;
constexpr int se_depth_margin = 3;
constexpr int se_mul = 100;
constexpr int double_margin = 19;

constexpr int lmr_depth = 2;
constexpr int lmr_quiet_history = 13500;
constexpr int asp_window = 20;
constexpr int asp_window_mul = 15;
constexpr int asp_window_max = 650;
constexpr int asp_depth = 8;

template <Color color, NodeType node_type>
std::int16_t alpha_beta(board& chessboard, search_data& data, std::int16_t alpha, std::int16_t beta, std::int8_t depth, bool cutnode) {
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
        alpha = std::max(alpha, static_cast<std::int16_t>(-20'000 + data.get_ply()));
        beta = std::min(beta, static_cast<std::int16_t>(20'000 - data.get_ply() - 1));
        if (alpha > beta) {
            return alpha;
        }

        if (chessboard.is_draw(data.get_ply())) {
            return 0;
        }

        in_check = chessboard.in_check();
    }

    if (depth <= 0) {
        return quiescence_search<color>(chessboard, data, alpha, beta);
    }

    Bound flag = Bound::UPPER;

    std::uint64_t zobrist_key = chessboard.get_hash_key();
    const TT_entry& tt_entry = tt.retrieve(zobrist_key, data.get_ply());

    chess_move best_move;
    chess_move tt_move = {};
    std::int16_t eval, static_eval, raw_eval;
    bool would_tt_prune = false;
    bool tt_pv = is_pv;

    if (tt_entry.zobrist == tt.upper(zobrist_key)) {
        best_move = tt_entry.tt_move;
        tt_move = tt_entry.tt_move;
        std::int16_t tt_eval = tt_entry.score;
        raw_eval = tt_entry.static_eval;
        eval = static_eval = history->correct_eval<color>(chessboard, data, raw_eval);
        tt_pv = tt_pv || tt_entry.tt_pv;

        if constexpr (!is_root) {
            if (tt_entry.depth >= depth + 2 * is_pv) {
                if ((tt_entry.bound == Bound::EXACT) ||
                    (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
                    (tt_entry.bound == Bound::UPPER && tt_eval <= alpha)) {
                    would_tt_prune = true;
                }
            }
        }

        if (would_tt_prune && data.singular_move[data.get_ply()] == 0) {
            if (is_pv) {
                depth --;
            } else {
                return tt_eval;
            }
        }

        if (!((eval > tt_eval && tt_entry.bound == Bound::LOWER) || (eval < tt_eval && tt_entry.bound == Bound::UPPER)))
        {
            eval = tt_eval;
        }
    } else {
        raw_eval = in_check ? -INF : evaluate<color>(chessboard);
        eval = static_eval = history->correct_eval<color>(chessboard, data, raw_eval);
        if (data.singular_move[data.get_ply()] == 0 && depth >= iir_depth) {
            depth--;
        }
    }

    data.improving[data.get_ply()] = static_eval;
    int improving = !in_check && data.get_ply() > 1 && static_eval > data.improving[data.get_ply() - 2];

    data.prev_moves[data.get_ply()] = {};
    data.reset_killers();
    data.singular_move[data.get_ply() + 1] = {};

    if constexpr (!is_root) {
        if (data.singular_move[data.get_ply()] == 0 && !in_check && std::abs(beta) < 9'000) {
            // razoring
            if (depth < razoring_depth && eval + razoring * depth <= alpha) {
                std::int16_t razor_eval = quiescence_search<color>(chessboard, data, alpha, beta);
                if (razor_eval <= alpha) {
                    return razor_eval;
                }
            }

            // reverse futility pruning
            if (depth < rfp_depth && eval - (rfp - 48 * !is_pv) * (depth - improving) >= beta) {
                return (eval + beta) / 2;
            }

            // NULL MOVE PRUNING
            if (node_type != NodeType::Null && depth >= nmp_depth && eval >= beta && static_eval >= beta && !chessboard.pawn_endgame()) {
                chessboard.make_null_move<color>();
                tt.prefetch(chessboard.get_hash_key());
                int R = nmp + depth / nmp_div + improving + std::min((static_eval - beta) / 245, 3);
                data.augment_ply();
                std::int16_t nullmove_score = -alpha_beta<enemy_color, NodeType::Null>(chessboard, data, -beta, -alpha, depth - R, !cutnode);
                data.reduce_ply();
                chessboard.undo_null_move<color>();
                if (nullmove_score >= beta) {
                    return std::abs(nullmove_score) > 19'000 ? beta : nullmove_score;
                }
            }

            const auto probcut_beta = beta + 214;
            if (depth >= 5 && !(tt_move.get_value() && tt_entry.depth > depth - 3 && tt_entry.score < probcut_beta)) {
                const auto see_treshold = probcut_beta - static_eval;
                move_list movelist;
                generate_all_moves<color, true>(chessboard, movelist);
                qs_score_moves(chessboard, movelist);

                for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
                    chess_move &chessmove = movelist.get_next_move(moves_searched);

                    if (!see<color>(chessboard, chessmove, see_treshold)) {
                        continue;
                    }

                    make_move<color>(chessboard, chessmove);
                    data.augment_ply();
                    tt.prefetch(chessboard.get_hash_key());
                    std::int16_t score = -quiescence_search<enemy_color>(chessboard, data, -probcut_beta,-probcut_beta + 1);

                    if (score >= probcut_beta) {
                        score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -probcut_beta,-probcut_beta + 1, depth - 3, !cutnode);
                    }

                    undo_move<color>(chessboard, chessmove);
                    data.reduce_ply();

                    if (score >= probcut_beta) {
                        tt.store(Bound::LOWER, std::int8_t(depth - 3), score, raw_eval, chessmove, data.get_ply(), tt_pv, zobrist_key);
                        return score;
                    }
                }
            }
        }
    }

    move_list movelist, quiets, captures;
    generate_all_moves<color, false>(chessboard, movelist);

    if (movelist.size() == 0) {
        if (data.singular_move[data.get_ply()] > 0) return alpha;
        if (in_check) {
            return data.mate_value();
        } else {
            return 0;
        }
    }

    std::int16_t best_score = -INF;
    score_moves<color>(chessboard, movelist, data, best_move);

    for (std::uint8_t moves_searched = 0; moves_searched < movelist.size(); moves_searched++) {
        chess_move& chessmove = movelist.get_next_move(moves_searched);

        if (chessmove.get_value() == data.singular_move[data.get_ply()]) {
            continue;
        }

        std::uint64_t start_nodes = data.get_nodes();

        int reduction = lmr_table[depth][moves_searched];
        bool is_quiet = chessboard.is_quiet(chessmove);

        if constexpr (!is_root) {
            if (moves_searched && best_score > -9'000 && !in_check && movelist[moves_searched] < 20'000) {
                if (is_quiet) {
                    if (quiets.size() > lmp_base + depth * depth / (2 - improving)) {
                        break;
                    }

                    int lmr_depth = std::max(0, depth - reduction - !improving + movelist.get_move_score(moves_searched) / 6000);
                    if (lmr_depth < fp_depth && static_eval + fp_base + fp_mul * lmr_depth <= alpha) {
                        break;
                    }
                }


                int see_margin = is_quiet ? -see_quiet * depth : -see_noisy * depth * depth;
                if (depth <= 6 + is_quiet * 4 && !see<color>(chessboard, chessmove, see_margin)) {
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
                data.singular_move[data.get_ply()] == 0)
            {
                int s_beta = tt_entry.score - se_mul * depth / 80;
                data.singular_move[data.get_ply()] = chessmove.get_value();
                int s_score = alpha_beta<color, NodeType::Non_PV>(chessboard, data, s_beta - 1, s_beta, (depth - 1) / 2, cutnode);
                data.singular_move[data.get_ply()] = 0;
                if (s_score < s_beta) {
                    ext = 1;
                    if constexpr(!is_pv) {
                        if (s_score + double_margin < s_beta) {
                            ext = 2 + (chessboard.is_quiet(chessmove) && s_score + 62 < s_beta);
                        }
                    }
                } else if (s_beta >= beta) {
                    return s_beta;
                } else if (tt_entry.score >= beta || cutnode) {
                    ext = -2;
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

        int new_depth = depth - 1 + ext;

        std::int16_t score;
        if (moves_searched == 0) {
            score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, new_depth, false);
        } else {
            // late move reduction
            if (depth >= lmr_depth) {
                if (is_quiet) {
                    reduction -= movelist.get_move_score(moves_searched) / lmr_quiet_history;
                } else {
                    reduction -= movelist.get_move_score(moves_searched) > 1'000'000;
                }

                reduction += !improving;
                reduction -= tt_pv;
                reduction += cutnode * 2;

                reduction = std::clamp(reduction, 0, depth - 2);
            } else {
                reduction = 0;
            }

            score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, new_depth - reduction, true);

            if (score > alpha && reduction > 0) {
                if constexpr (!is_root) {
                    new_depth += (score > best_score + 80);
                    new_depth -= (score < best_score + new_depth);
                }

                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, new_depth, !cutnode);
            }

            if (is_pv && score > alpha) {
                score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, new_depth, false);
            }
        }

        undo_move<color>(chessboard, chessmove);
        data.reduce_ply();

        if constexpr (is_root) {
            data.update_node_count(from, to, start_nodes);
        }

        if (score > best_score) {
            best_score = score;
            data.update_pv(chessmove);
            if constexpr (is_root) {
                data.best_move = chessmove.to_string();
            }

            if (score > alpha) {
                alpha = score;
                flag = Bound::EXACT;
                best_move = chessmove;

                if (alpha >= beta) {
                    flag = Bound::LOWER;
                    if (is_quiet) {
                        data.update_killer(chessmove);
                    }
                    history->update<color, is_root>(data, chessboard, best_move, quiets, captures, depth + (best_score > beta + 80));
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

    if (data.singular_move[data.get_ply()] == 0) {
        int avg_eval = (raw_eval + static_eval * 2) / 3;
        if (!(in_check || !(best_move.get_value() == 0 || chessboard.is_quiet(best_move))
              || (flag == Bound::LOWER && best_score <= avg_eval) || (flag == Bound::UPPER && best_score >= avg_eval))
        ) {
            history->update_correction_history<color>(chessboard, data, best_score, avg_eval, depth);
        }

        if (!would_tt_prune) {
            tt.store(flag, depth, best_score, raw_eval, best_move, data.get_ply(), tt_pv, zobrist_key);
        }
    }

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
        score = alpha_beta<color, NodeType::Root>(chessboard, data, alpha, beta, search_depth, false);
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

        window += window / 3;
        if (window > asp_window_max) {
            window = INF;
        }
    }

    tt.increase_age();
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
            score = alpha_beta<color, NodeType::Root>(chessboard, data, -10'000, 10'000, depth, false);
        } else {
            score = aspiration_window<color>(chessboard, data, score, depth);
        }

        if (depth > 1 && data.time_stopped()) {
            break;
        }

        std::string score_string = " score cp " + std::to_string(score);

        if (std::abs(score) >= 19'000) {
            score_string = " score mate " + std::to_string(score > 0 ? (20'000 - score + 1) / 2 : -(20'000 + score) / 2);
        }

        std::cout << "info depth " << depth << score_string << " nodes " << data.nodes() << " nps " << data.nps() << " pv " << data.get_pv(depth) << std::endl;
        data.reset_nodes();

        best_move = data.best_move;
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board& chessboard, time_info& info) {
    search_data data;

    if (chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo, chessboard.move_count(), info.max_nodes);
        iterative_deepening<White>(chessboard, data, info.max_depth);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo, chessboard.move_count(), info.max_nodes);
        iterative_deepening<Black>(chessboard, data, info.max_depth);
    }
}

#endif //MOTOR_SEARCH_HPP