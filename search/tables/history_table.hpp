#ifndef MOTOR_BUTTERFLY_TABLE_HPP
#define MOTOR_BUTTERFLY_TABLE_HPP

#include <array>
#include "../../chess_board/board.hpp"
#include "../../chess_board/chess_move.hpp"
#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"

std::array<std::array<std::array<int, 64>, 64>, 2> history_table = {};
std::array<std::array<std::array<int, 64>, 6>, 16384> pawn_table = {};
std::array<std::array<std::array<std::array<int, 64>, 6>, 64>, 6> continuation_table = {};
std::array<std::array<std::array<int, 7>, 64>, 6> capture_table = {};

constexpr int noisy_mul = 30;
constexpr int noisy_max = 500;
constexpr int noisy_gravity = 2048;
constexpr int quiet_mul = 200;
constexpr int quiet_max = 2000;

int history_bonus(int depth) {
    return std::min(quiet_max, quiet_mul * depth);
}

void update_history(int& value, int bonus) {
    value += bonus - (value * std::abs(bonus) / 16384);
}

void update_cap_history(int& value, int bonus) {
    value += bonus - (value * std::abs(bonus) / noisy_gravity);
}

template <Color color, bool is_root>
void update_quiet_history(search_data & data, board & chessboard, const chess_move & best_move, move_list & quiets, move_list & captures, int depth) {
    int bonus = history_bonus(depth);
    int cap_bonus = std::min(noisy_max, noisy_mul * depth);

    auto [piece, from, to] = data.prev_moves[data.get_ply()];
    history_move prev = {}, prev2 = {}, prev4 = {};
    const std::uint64_t pawn_key = chessboard.get_pawn_key() & 0b11111111111111;

    if (chessboard.is_quiet(best_move)) {

        update_history(history_table[color][from][to], bonus);
        update_history(pawn_table[pawn_key][piece][to], bonus);

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
            int malus = -bonus;
            auto qfrom = quiet.get_from();
            auto qto = quiet.get_to();
            auto qpiece = chessboard.get_piece(qfrom);
            update_history(history_table[color][qfrom][qto], malus);
            update_history(pawn_table[pawn_key][qpiece][qto], bonus);

            if constexpr (!is_root) {
                update_history(continuation_table[prev.piece_type][prev.to][qpiece][qto], malus);
                if (data.get_ply() > 1) {
                    update_history(continuation_table[prev2.piece_type][prev2.to][qpiece][qto], malus);
                    if (data.get_ply() > 3) {
                        update_history(continuation_table[prev4.piece_type][prev4.to][qpiece][qto], malus);
                    }
                }
            }
        }
    } else {
         update_cap_history(capture_table[piece][to][chessboard.get_piece(to)], cap_bonus);
    }

    for (const auto &capture: captures) {
        int malus = -cap_bonus;
        auto cap_to = capture.get_to();
        update_cap_history(capture_table[chessboard.get_piece(capture.get_from())][cap_to][chessboard.get_piece(cap_to)], malus);
    }
}

template <Color color>
int get_history(board & chessboard, search_data & data, Square from, Square to, Piece piece) {
    const std::uint64_t pawn_key = chessboard.get_pawn_key() & 0b11111111111111;
    int move_score = history_table[color][from][to];
    move_score += pawn_table[pawn_key][piece][to] - 400;
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
