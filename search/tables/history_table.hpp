#ifndef HISTORY_HPP
#define HISTORY_HPP

#include <array>
#include <memory>
#include <algorithm>
#include "../../chess_board/board.hpp"
#include "../../chess_board/chess_move.hpp"
#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"

auto murmur_hash_3(std::uint64_t key) -> std::uint64_t {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

class History {
public:
    History()
            : history_table({}), continuation_table({}), capture_table({}),
              pawn_correction_table({}), nonpawn_correction_table({}), minor_correction_table({}), major_correction_table({}),
              threat_correction_table({}), continuation_correction_table({}), continuation_correction_table2({}) {}

    void clear() {
        history_table = {};
        continuation_table = {};
        capture_table = {};
        pawn_correction_table = {};
        nonpawn_correction_table = {};
        minor_correction_table = {};
        major_correction_table = {};
        threat_correction_table = {};
        continuation_correction_table = {};
        continuation_correction_table2 = {};
    }

    template <Color color, bool is_root>
    void update(search_data &data, board &chessboard, const chess_move &best_move, move_list &quiets, move_list &captures, int depth, int material_key) {
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
                update_history(continuation_table[color][prev.piece_type][prev.to][piece][to], bonus);
                if (data.get_ply() > 1) {
                    prev2 = data.prev_moves[data.get_ply() - 2];
                    update_history(continuation_table[color][prev2.piece_type][prev2.to][piece][to], bonus);
                    if (data.get_ply() > 3) {
                        prev4 = data.prev_moves[data.get_ply() - 4];
                        update_history(continuation_table[color][prev4.piece_type][prev4.to][piece][to], bonus);
                    }
                }
            }

            for (const auto &quiet : quiets) {
                auto qfrom = quiet.get_from();
                auto qto = quiet.get_to();
                auto qpiece = chessboard.get_piece(qfrom);
                bool qthreat_from = (threats & bb(qfrom));
                bool qthreat_to = (threats & bb(qto));
                update_history(history_table[color][qthreat_from][qthreat_to][qfrom][qto], penalty);

                if constexpr (!is_root) {
                    update_history(continuation_table[color][prev.piece_type][prev.to][qpiece][qto], penalty);
                    if (data.get_ply() > 1) {
                        update_history(continuation_table[color][prev2.piece_type][prev2.to][qpiece][qto], penalty);
                        if (data.get_ply() > 3) {
                            update_history(continuation_table[color][prev4.piece_type][prev4.to][qpiece][qto], penalty);
                        }
                    }
                }
            }
        } else {
            update_cap_history(capture_table[piece][to][chessboard.get_piece(to)], bonus);
        }

        for (const auto &capture : captures) {
            auto cap_to = capture.get_to();
            update_cap_history(capture_table[chessboard.get_piece(capture.get_from())][cap_to][chessboard.get_piece(cap_to)], penalty);
        }
    }

    template <Color color>
    int get_quiet_score(board &chessboard, const search_data &data, Square from, Square to, Piece piece, int material_key) const {
        std::uint64_t threats = chessboard.get_threats();
        bool threat_from = (threats & bb(from));
        bool threat_to = (threats & bb(to));

        int move_score = history_table[color][threat_from][threat_to][from][to];

        int ply = data.get_ply();
        if (ply > 0) {
            auto prev = data.prev_moves[ply - 1];
            move_score += continuation_table[color][prev.piece_type][prev.to][piece][to];
            if (ply > 1) {
                auto prev2 = data.prev_moves[ply - 2];
                move_score += continuation_table[color][prev2.piece_type][prev2.to][piece][to];
                if (ply > 3) {
                    auto prev4 = data.prev_moves[ply - 4];
                    move_score += continuation_table[color][prev4.piece_type][prev4.to][piece][to];
                }
            }
        }

        return move_score;
    }

    int get_capture_score(Piece from_piece, Square to, Piece to_piece) const {
        return capture_table[from_piece][to][to_piece];
    }

    template<Color color>
    void update_correction_history(board& chessboard, const search_data &data, int best_score, int raw_eval, int depth) {

        int diff = (best_score - raw_eval) * 256;
        int weight = std::min(128, depth * (depth + 1));

        constexpr int CORRECTION_CLAMP_MIN = -12'288;
        constexpr int CORRECTION_CLAMP_MAX = 12'288;
        constexpr int CORRECTION_TABLE_SIZE = 16384;

        auto update_entry = [&](int& entry) {
            entry = (entry * (256 - weight) + diff * weight) / 256;
            entry = std::clamp(entry, CORRECTION_CLAMP_MIN, CORRECTION_CLAMP_MAX);
        };

        update_entry(pawn_correction_table[color][chessboard.get_pawn_key() % CORRECTION_TABLE_SIZE]);

        std::uint64_t threat_key = murmur_hash_3(chessboard.get_threats() & chessboard.get_side_occupancy<color>());
        update_entry(threat_correction_table[color][threat_key % CORRECTION_TABLE_SIZE]);

        auto [wkey, bkey] = chessboard.get_nonpawn_key();
        update_entry(nonpawn_correction_table[color][White][wkey % CORRECTION_TABLE_SIZE]);
        update_entry(nonpawn_correction_table[color][Black][bkey % CORRECTION_TABLE_SIZE]);

        update_entry(minor_correction_table[color][chessboard.get_minor_key() % CORRECTION_TABLE_SIZE]);
        update_entry(major_correction_table[color][chessboard.get_major_key() % CORRECTION_TABLE_SIZE]);

        if (data.get_ply() > 1) {
            auto prev1 = data.prev_moves[data.get_ply() - 1];
            auto prev2 = data.prev_moves[data.get_ply() - 2];
            update_entry(continuation_correction_table[prev2.piece_type][prev2.to][prev1.piece_type][prev1.to]);

            if (data.get_ply() > 2) {
                auto prev3 = data.prev_moves[data.get_ply() - 3];
                update_entry(continuation_correction_table2[prev3.piece_type][prev3.to][prev1.piece_type][prev1.to]);
            }
        }
    }

    template <Color color>
    std::int16_t correct_eval(const board &chessboard, const search_data &data, int raw_eval) {
        if (std::abs(raw_eval) > 8'000) return raw_eval;
        std::uint64_t threat_key = murmur_hash_3(chessboard.get_threats() & chessboard.get_side_occupancy<color>());

        const int pawn_entry = pawn_correction_table[color][chessboard.get_pawn_key() % 16384];
        const int threat_entry = threat_correction_table[color][threat_key % 16384];
        const int minor_entry = minor_correction_table[color][chessboard.get_minor_key() % 16384];
        const int major_entry = major_correction_table[color][chessboard.get_major_key() % 16384];

        auto [wkey, bkey] = chessboard.get_nonpawn_key();
        const int nonpawn_entry = nonpawn_correction_table[color][White][wkey % 16384] + nonpawn_correction_table[color][Black][bkey % 16384];

        int cont_entry = 0;
        int cont_entry2 = 0;
        if (data.get_ply() > 1) {
            auto prev1 = data.prev_moves[data.get_ply() - 1];
            auto prev2 = data.prev_moves[data.get_ply() - 2];
            cont_entry = continuation_correction_table[prev2.piece_type][prev2.to][prev1.piece_type][prev1.to];
            if (data.get_ply() > 2) {
                auto prev3 = data.prev_moves[data.get_ply() - 3];
                cont_entry2 = continuation_correction_table2[prev3.piece_type][prev3.to][prev1.piece_type][prev1.to];
            }
        }

        return raw_eval + (pawn_entry * 200 + threat_entry * 100 + nonpawn_entry * 200 + minor_entry * 150 + major_entry * 120 + cont_entry * 180 + cont_entry2 * 180) / (256 * 300);
    }


private:
    std::array<std::array<std::array<std::array<std::array<int, 64>, 64>, 2>, 2>, 2> history_table;
    std::array<std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7>, 2> continuation_table;
    std::array<std::array<std::array<int, 7>, 64>, 6> capture_table;
    std::array<std::array<int, 16384>, 2> pawn_correction_table;
    std::array<std::array<std::array<int, 16384>, 2>, 2> nonpawn_correction_table;
    std::array<std::array<int, 16384>, 2> minor_correction_table;
    std::array<std::array<int, 16384>, 2> major_correction_table;
    std::array<std::array<int, 16384>, 2> threat_correction_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_correction_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_correction_table2;

    int history_bonus(int depth) const {
        return std::min(2040, 236 * depth);
    }

    void update_history(int &value, int bonus) const {
        value += bonus - (value * std::abs(bonus) / 16384);
    }

    void update_cap_history(int &value, int bonus) const {
        constexpr int noisy_gravity = 16384;
        value += bonus - (value * std::abs(bonus) / noisy_gravity);
    }
};

std::unique_ptr<History> history = std::make_unique<History>();

#endif // HISTORY_HPP
