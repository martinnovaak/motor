#ifndef MOTOR_BUTTERFLY_TABLE_HPP
#define MOTOR_BUTTERFLY_TABLE_HPP

#include <array>
#include "../../chess_board/board.hpp"
#include "../../chess_board/chess_move.hpp"
#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"

std::array<std::array<std::array<std::array<std::array<int, 64>, 64>, 2>, 2>, 2> history_table = {};
std::array<std::array<std::array<std::array<int, 64>, 6>, 64>, 6> continuation_table = {};
std::array<std::array<std::array<int, 7>, 64>, 6> capture_table = {};
std::array<std::array<int, 16384>, 2> correction_table = {};

constexpr int history_mul = 236;
constexpr int history_max = 2040;

int history_bonus(int depth) {
    return std::min(history_max, history_mul * depth);
}

void update_history(int& value, int bonus) {
    value += bonus - (value * std::abs(bonus) / 16384);
}

template <Color color, bool is_root>
void update_history(search_data & data, board & chessboard, const chess_move & best_move, move_list & quiets, move_list & captures, int depth) {
    int bonus = history_bonus(depth);
    int penalty = -bonus;

    auto [piece, from, to] = data.prev_moves[data.get_ply()];
    history_move prev = {}, prev2 = {}, prev4 = {};

    std::uint64_t threats = chessboard.get_threats();

    if (chessboard.is_quiet(best_move)) {
        bool threat_from = (threats & bb(from));
        bool threat_to = (threats & bb(to));
        update_history(history_table[color][threat_from][threat_to][from][to], bonus);

        if constexpr (!is_root) {
            prev = data.prev_moves[data.get_ply() - 1];
            update_history(continuation_table[prev.piece_type][prev.to][piece][to], bonus);
            if (data.get_ply() > 1) {
                prev2 = data.prev_moves[data.get_ply() - 2];
                update_history(continuation_table[prev2.piece_type][prev2.to][piece][to], bonus);
                if (data.get_ply() > 3) {
                    prev4 = data.prev_moves[data.get_ply() - 4];
                    update_history(continuation_table[prev4.piece_type][prev4.to][piece][to], bonus);
                }
            }
        }

        for (const auto &quiet: quiets) {
            auto qfrom = quiet.get_from();
            auto qto = quiet.get_to();
            auto qpiece = chessboard.get_piece(qfrom);
            bool qthreat_from = (threats & bb(qfrom));
            bool qthreat_to = (threats & bb(qto));
            update_history(history_table[color][qthreat_from][qthreat_to][qfrom][qto], penalty);

            if constexpr (!is_root) {
                update_history(continuation_table[prev.piece_type][prev.to][qpiece][qto], penalty);
                if (data.get_ply() > 1) {
                    update_history(continuation_table[prev2.piece_type][prev2.to][qpiece][qto], penalty);
                    if (data.get_ply() > 3) {
                        update_history(continuation_table[prev4.piece_type][prev4.to][qpiece][qto], penalty);
                    }
                }
            }
        }
    } else {
        update_history(capture_table[piece][to][chessboard.get_piece(to)], bonus);
    }

    for (const auto &capture: captures) {
        auto cap_to = capture.get_to();
        update_history(capture_table[chessboard.get_piece(capture.get_from())][cap_to][chessboard.get_piece(cap_to)], penalty);
    }
}

template <Color color>
int get_history(board & chessboard, search_data & data, Square from, Square to, Piece piece) {
    std::uint64_t threats = chessboard.get_threats();
    bool threat_from = (threats & bb(from));
    bool threat_to = (threats & bb(to));

    int move_score = history_table[color][threat_from][threat_to][from][to];
    if (data.get_ply()) {
        auto prev = data.prev_moves[data.get_ply() - 1];
        move_score += continuation_table[prev.piece_type][prev.to][piece][to];
        if (data.get_ply()) {
            prev = data.prev_moves[data.get_ply() - 2];
            move_score += continuation_table[prev.piece_type][prev.to][piece][to];
            if (data.get_ply()) {
                prev = data.prev_moves[data.get_ply() - 4];
                move_score += continuation_table[prev.piece_type][prev.to][piece][to];
            }
        }
    }

    return move_score;
}

#endif //MOTOR_BUTTERFLY_TABLE_HPP
