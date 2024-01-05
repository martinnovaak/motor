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

int rfp = 150;
int razoring = 200;
int iid = 6;
double lmr = 5.5;
int nmp_numerator = 2;
int nmp_denominator = 6;

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
std::int16_t alpha_beta(board & chessboard, search_data & data, std::int16_t alpha, std::int16_t beta, std::int8_t depth) {
    constexpr Color enemy_color = color == White ? Black : White;
    constexpr bool is_pv = node_type == NodeType::PV || node_type == NodeType::Root;
    constexpr bool is_root = node_type == NodeType::Root;

    if(data.should_end()) {
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
    const TT_entry & tt_entry = tt[zobrist_key];
    chess_move best_move;

    std::int16_t eval = evaluate<color>(chessboard);;

    bool tt_hit = false;
    if (tt_entry.zobrist == zobrist_key) {
        best_move = tt_entry.tt_move;
        tt_hit = true;
        std::int16_t tt_eval = tt_entry.score;
        if constexpr (!is_pv) {
            if (tt_entry.depth >= depth) {
                switch (tt_entry.bound) {
                    case Bound::EXACT:
                        return tt_eval;
                    case Bound::LOWER:
                        if (tt_eval >= beta) {
                            return tt_eval;
                        }
                        break;
                    case Bound::UPPER:
                        if (tt_eval <= alpha) {
                            return tt_eval;
                        }
                        break;
                }
            }
        }
    } else {
        if (depth >= 4) {
            depth--;
            if constexpr (is_pv) {
                depth--;
            }
        }
    }

    int improving = 1 + data.improving(eval);
    if constexpr (!is_root) {
        if (!in_check && std::abs(beta) < 9'000) {
            // reverse futility pruning
            if (depth < 7 && eval - rfp * depth >= beta) {
                return beta;
            }

            // razoring
            if (depth <= 6 && eval + razoring * depth <= alpha) {
                std::int16_t razor_eval = quiescence_search<color>(chessboard, data, alpha, beta);
                if(razor_eval <= alpha) {
                    return razor_eval;
                }
            }

            // NULL MOVE PRUNING
            if (node_type != NodeType::Null && depth >= 3 && eval >= beta && !chessboard.pawn_endgame()) {
                chessboard.make_null_move<color>();
                int R = 3 + depth * nmp_numerator / nmp_denominator;
                data.augment_ply();
                std::int16_t nullmove_score = -alpha_beta<enemy_color, NodeType::Null>(chessboard, data, -beta, -alpha,depth - R);
                data.reduce_ply();
                chessboard.undo_null_move<color>();
                if (nullmove_score >= beta) {
                    return nullmove_score;
                }
            }
        }
    }

    move_list movelist, quiets;
    generate_all_moves<color, false>(chessboard, movelist);

    if (movelist.size() == 0) {
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
        chess_move & chessmove = movelist.get_next_move(moves_searched);

        if constexpr (!is_pv) {
            if (best_score > -9'000 && !in_check && chessmove.get_score() < 15'000) {
                if (moves_searched > 3 + 2 * depth * depth / improving) {
                    break;
                }
            }
        }

        make_move<color>(chessboard, chessmove);
        data.augment_ply();
        data.eval_grandfather = data.eval_father;
        data.eval_father = eval;

        std::int16_t score;
        if (moves_searched == 0) {
            score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1);
        } else {
            // late move reduction
            score = alpha + 1;
            if(depth >= 3 && chessmove.get_score() < 15'000 && chessmove.get_check_type() == Check_type::NOCHECK) {
                int reduction = 2 + std::log2(depth) * std::log2(moves_searched) / lmr;
                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - reduction);
            }

            if (score > alpha) {
                score = -alpha_beta<enemy_color, NodeType::Non_PV>(chessboard, data, -alpha - 1, -alpha, depth - 1);
                if (score > alpha && score < beta) {
                    score = -alpha_beta<enemy_color, NodeType::PV>(chessboard, data, -beta, -alpha, depth - 1);
                }
            }
        }

        if (data.time_is_up()) {
            return 0;
        }

        undo_move<color>(chessboard);
        data.reduce_ply();

        if (score > best_score) {
            best_score = score;
            data.update_pv(chessmove);

            if (score > alpha) {
                alpha = score;
                flag = Bound::EXACT;

                if (alpha >= beta) {
                    flag = Bound::LOWER;
                    if (chessmove.is_quiet()) {
                        data.update_killer(chessmove);
                        data.update_history(chessmove.get_from(), chessmove.get_to(), depth, quiets.size());
                        data.counter_moves[previous_move.get_from()][previous_move.get_to()] = chessmove;

                        int i = 0;
                        for (const auto & quiet : quiets) {
                            data.reduce_history(quiet.get_from(), quiet.get_to(), depth, quiets.size() - i);
                            i++;
                        }
                    }
                    break;
                }
            }
        }
        if (chessmove.is_quiet()) {
            quiets.add(chessmove);
        }
    }

    tt[zobrist_key] = {flag, depth, best_score, best_move, zobrist_key};
    return best_score;
}

template <Color color>
std::int16_t aspiration_window(board & chessboard, search_data & data, std::int16_t score, int depth) {
    std::int16_t alpha_window = 20, beta_window = 20;
    std::int16_t alpha, beta;

    while(!data.time_is_up()) {
        alpha = std::max(static_cast<std::int16_t>(-INF), static_cast<std::int16_t>(score  - alpha_window));
        beta  = std::min( INF, static_cast<std::int16_t>(score  + beta_window));

        score = alpha_beta<color, NodeType::Root>(chessboard, data, alpha, beta, depth);
        if (score <= alpha) {
            alpha_window *= 3;
            beta_window *= 2;
        } else if (score >= beta) {
            alpha_window *= 2;
            beta_window *= 3;
            //depth = std::max(1, depth - 1);
        } else {
            break;
        }
    }

    return score;
}

template <Color color>
void iterative_deepening(board & chessboard, search_data & data) {
    std::string best_move;
    int score;
    for (int depth = 1; depth < MAX_DEPTH; depth++) {
        if (depth < 6) {
            score = alpha_beta<color, NodeType::Root>(chessboard, data, -10'000, 10'000, depth);
        } else {
            score = aspiration_window<color>(chessboard, data, score, depth);
        }

        if (data.time_is_up()) {
            break;
        }

        // std::cout << depth << ": " << score << std::endl;
        best_move = data.get_best_move();
    }
    std::cout << "bestmove " << best_move << "\n";
}

void find_best_move(board & chessboard, time_info & info) {
    search_data data;
    history_table.centralize_whole_table();
    if(chessboard.get_side() == White) {
        data.set_timekeeper(info.wtime, info.winc, info.movestogo);
        iterative_deepening<White>(chessboard, data);
    } else {
        data.set_timekeeper(info.btime, info.binc, info.movestogo);
        iterative_deepening<Black>(chessboard, data);
    }
}

#endif //MOTOR_SEARCH_HPP
