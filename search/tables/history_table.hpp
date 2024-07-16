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

constexpr int noisy_mul = 41;
constexpr int noisy_max = 375;
constexpr int noisy_gravity = 1779;
constexpr int quiet_mul = 236;
constexpr int quiet_max = 2040;

int history_bonus(int depth) {
    return std::min(quiet_max, quiet_mul * depth);
}

void update_history(int& value, int bonus) {
    value += bonus - (value * std::abs(bonus) / 16384);
}

void update_cap_history(int& value, int bonus) {
    value += bonus - (value * std::abs(bonus) / noisy_gravity);
}

template <Color color>
void update_history(search_data & data, board & chessboard, const chess_move & best_move, move_list & quiets, move_list & captures, int depth) {
    int bonus = history_bonus(depth);
    int cap_bonus = std::min(noisy_max, noisy_mul * depth);

    auto [piece, from, to] = data.prev_moves[data.get_ply()];
    std::uint64_t threats = chessboard.get_threats();

    std::vector<int> ply_offsets = {1, 2, 4};

    auto update_continuation_table = [&](int bonus_or_malus) {
        for (int ply_offset : ply_offsets) {
            if (data.get_ply() >= ply_offset) {
                auto prev = data.prev_moves[data.get_ply() - ply_offset];
                update_history(continuation_table[prev.piece_type][prev.to][piece][to], bonus_or_malus);
            }
        }
    };

    if (chessboard.is_quiet(best_move)) {
        bool threat_from = (threats & bb(from));
        bool threat_to = (threats & bb(to));
        update_history(history_table[color][threat_from][threat_to][from][to], bonus);
        update_continuation_table(bonus);

        for (const auto &quiet : quiets) {
            int penalty = -bonus;
            auto qfrom = quiet.get_from();
            auto qto = quiet.get_to();
            bool qthreat_from = (threats & bb(qfrom));
            bool qthreat_to = (threats & bb(qto));
            update_history(history_table[color][qthreat_from][qthreat_to][qfrom][qto], penalty);
            update_continuation_table(penalty);
        }
    } else {
        update_cap_history(capture_table[piece][to][chessboard.get_piece(to)], cap_bonus);
    }

    for (const auto &capture : captures) {
        int penalty = -cap_bonus;
        auto cap_from = capture.get_from();
        auto cap_to = capture.get_to();
        auto cap_piece = chessboard.get_piece(cap_from);
        update_cap_history(capture_table[cap_piece][cap_to][chessboard.get_piece(cap_to)], penalty);
    }
}

template <Color color>
int get_history(board & chessboard, search_data & data, Square from, Square to, Piece piece) {
    std::uint64_t threats = chessboard.get_threats();
    bool threat_from = (threats & bb(from));
    bool threat_to = (threats & bb(to));

    int move_score = 2 * history_table[color][threat_from][threat_to][from][to];

    std::vector<std::pair<int, int>> ply_offsets = {{1, 2}, {2, 1}, {4, 1}};

    for (const auto& [ply_offset, weight] : ply_offsets) {
        if (data.get_ply() >= ply_offset) {
            auto prev = data.prev_moves[data.get_ply() - ply_offset];
            move_score += weight * continuation_table[prev.piece_type][prev.to][piece][to];
        }
    }

    return move_score;
}

#endif //MOTOR_BUTTERFLY_TABLE_HPP
