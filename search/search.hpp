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

template <Color color>
std::int16_t correct_eval(const board & chessboard, search_data& data, int raw_eval) {
    if (std::abs(raw_eval) > 8'000) return raw_eval;
    const int entry = correction_table[color][chessboard.get_pawn_key() % 16384];
    const int material_entry = material_correction_table[color][chessboard.get_material_key() % 32768];
    auto [wkey, bkey] = chessboard.get_nonpawn_key();
    const int nonpawn_entry = nonpawn_correction_table[color][White][wkey % 16384] + nonpawn_correction_table[color][Black][bkey % 16384];
    return raw_eval + (entry + material_entry + nonpawn_entry / 2) / 256;
}

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
        if (chessboard.is_draw()) {
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

    if (data.singular_move == 0 && tt_entry.zobrist == tt.upper(zobrist_key)) {
        best_move = tt_entry.tt_move;
        tt_move = tt_entry.tt_move;
        std::int16_t tt_eval = tt_entry.score;
        raw_eval = tt_entry.static_eval;
        eval = static_eval = correct_eval<color>(chessboard, data, raw_eval);

        if constexpr (!is_root) {
            if (tt_entry.depth >= depth + 2 * is_pv) {
                if ((tt_entry.bound == Bound::EXACT) ||
                    (tt_entry.bound == Bound::LOWER && tt_eval >= beta) ||
                    (tt_entry.bound == Bound::UPPER && tt_eval <= alpha)) {
                    would_tt_prune = true;
                }
            }
        }

        if (would_tt_prune) {
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
        eval = static_eval = correct_eval<color>(chessboard, data, raw_eval);
        if (data.singular_move == 0 && depth >= iir_depth) {
            depth--;
        }
    }

    data.improving[data.get_ply()] = static_eval;
    int improving = !in_check && data.get_ply() > 1 && static_eval > data.improving[data.get_ply() - 2];

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
            if (node_type != NodeType::Null && depth >= nmp_depth && eval >= beta && static_eval >= beta && !chessboard.pawn_endgame()) {
                chessboard.make_null_move<color>();
                tt.prefetch(chessboard.get_hash_key());
                int R = nmp + depth / nmp_div + improving + std::min((static_eval - beta) / 256, 3);
                data.augment_ply();
                std::int16_t nullmove_score = -alpha_beta<enemy_color, NodeType::Null>(chessboard, data, -beta, -alpha, depth - R, !cutnode);
                data.reduce_ply();
                chessboard.undo_null_move<color>();
                if (nullmove_score >= beta) {
                    return std::abs(nullmove_score) > 19'000 ? beta : nullmove_score;
                }
            }

            const auto probcut_beta = beta + 250;
            if (depth >= 6 && !(tt_move.get_value() && tt_entry.depth > depth - 3 && tt_entry.score < probcut_beta)) {
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
                        tt.store(Bound::LOWER, std::int8_t(depth - 3), score, raw_eval, chessmove, data.get_ply(), zobrist_key);
                        return score;
                    }
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

                    int lmr_depth = std::max(0, depth - reduction - !improving + movelist.get_move_score(moves_searched) / 6000);
                    if (lmr_depth < fp_depth && static_eval + fp_base + fp_mul * lmr_depth <= alpha) {
                        continue;
                    }
                }


                int see_margin = is_quiet ? -see_quiet * depth : -see_noisy * depth * depth;
                if (!see<color>(chessboard, chessmove, see_margin)) {
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
                data.singular_move == 0)
            {
                int s_beta = tt_entry.score - se_mul * depth / 80;
                data.singular_move = chessmove.get_value();
                int s_score = alpha_beta<color, NodeType::Non_PV>(chessboard, data, s_beta - 1, s_beta, (depth - 1) / 2, cutnode);
                data.singular_move = 0;
                if (s_score < s_beta) {
                    ext = 1;
                    if constexpr(!is_pv) {
                        if (s_score + double_margin < s_beta && data.double_extension[data.get_ply()] < double_exts) {
                            ext = 2;
                            data.double_extension[data.get_ply()]++;
                        }
                    }
                } else if (s_beta >= beta) {
                    return s_beta;
                } else if (cutnode) {
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
            if (depth >= lmr_depth && movelist.get_move_score(moves_searched) < 1'000'000) {
                if (is_quiet) {
                    reduction += !is_pv + !improving;
                    reduction -= chessboard.in_check();
                    reduction -= movelist.get_move_score(moves_searched) / lmr_quiet_history;
                    reduction += cutnode * 2;
                }

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
                    if (is_quiet) {
                        data.update_killer(chessmove);
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

    if (data.singular_move == 0) {
        if (!(in_check || !chessboard.is_quiet(best_move)
              || (flag == Bound::LOWER && best_score <= static_eval) || (flag == Bound::UPPER && best_score >= static_eval))
        ) {
            int diff = (best_score - raw_eval) * 256;
            int weight = std::min(16, depth + 1);

            int & entry = correction_table[color][chessboard.get_pawn_key() % 16384];
            entry = (entry * (256 - weight) + diff * weight) / 256;
            entry = std::clamp(entry, -8'192, 8'192);

            int & material_entry = material_correction_table[color][chessboard.get_material_key() % 32768];
            material_entry = (material_entry * (256 - weight) + diff * weight) / 256;
            material_entry = std::clamp(material_entry, -8'192, 8'192);

            auto [wkey, bkey] = chessboard.get_nonpawn_key();
            int & white_nonpawn_entry = nonpawn_correction_table[color][White][wkey % 16384];
            white_nonpawn_entry = (white_nonpawn_entry * (256 - weight) + diff * weight) / 256;
            white_nonpawn_entry = std::clamp(white_nonpawn_entry, -8'192, 8'192);

            int & black_nonpawn_entry = nonpawn_correction_table[color][Black][bkey % 16384];
            black_nonpawn_entry = (black_nonpawn_entry * (256 - weight) + diff * weight) / 256;
            black_nonpawn_entry = std::clamp(black_nonpawn_entry, -8'192, 8'192);
        }

        if (!would_tt_prune) {
            tt.store(flag, depth, best_score, raw_eval, best_move, data.get_ply(), zobrist_key);
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

        window += window * asp_window_mul / 32;
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

        std::cout << "info depth " << depth << " score cp " << score << " nodes " << data.nodes() << " nps " << data.nps() << " pv " << data.get_pv(depth) << std::endl;
        data.reset_nodes();

        best_move = data.best_move;

        if (depth > 20 && std::abs(score) > 19'900) {
            break;
        }
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