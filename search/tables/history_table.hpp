#ifndef HISTORY_HPP
#define HISTORY_HPP

#include <array>
#include <memory>
#include <algorithm>
#include "../tuning_options.hpp"
#include "../../chess_board/board.hpp"
#include "../../chess_board/chess_move.hpp"
#include "../../move_generation/move_list.hpp"
#include "../search_data.hpp"

TuningOption pawn_weight("pawn_weight", 192, 0, 400);
TuningOption nonpawn_weight("nonpawn_weight", 85, 0, 400);
TuningOption threat_weight("threat_weight", 88, 0, 400);
TuningOption major_weight("major_weight", 84, 0, 400);
TuningOption minor_weight("minor_weight", 146, 0, 400);
TuningOption mat_weight("mat_weight", 100, 0, 400);
TuningOption concorrde_weight("concorrde_weight", 150, 0, 400);

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
            : history_table({}), material_history_table({}), continuation_table({}), capture_table({}),
              correction_table({}), nonpawn_correction_table({}), minor_correction_table({}),
              major_correction_table({}), threat_correction_table({}), material_correction_table({}), continuation_correction_table({}) {}

    void clear() {
        history_table = {};
        material_history_table = {};
        continuation_table = {};
        capture_table = {};
        correction_table = {};
        nonpawn_correction_table = {};
        minor_correction_table = {};
        major_correction_table = {};
        threat_correction_table = {};
        material_correction_table = {};
        continuation_correction_table = {};
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
            update_history(material_history_table[material_key][color][piece][to], bonus);

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

            for (const auto &quiet : quiets) {
                auto qfrom = quiet.get_from();
                auto qto = quiet.get_to();
                auto qpiece = chessboard.get_piece(qfrom);
                bool qthreat_from = (threats & bb(qfrom));
                bool qthreat_to = (threats & bb(qto));
                update_history(history_table[color][qthreat_from][qthreat_to][qfrom][qto], penalty);
                update_history(material_history_table[material_key][color][qpiece][qto], penalty);

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

        int move_score = 94 * history_table[color][threat_from][threat_to][from][to] / 100;
        move_score += material_history_table[material_key][color][piece][to];

        int ply = data.get_ply();
        if (ply > 0) {
            auto prev = data.prev_moves[ply - 1];
            move_score += 103 * continuation_table[prev.piece_type][prev.to][piece][to] / 100;
            if (ply > 1) {
                auto prev2 = data.prev_moves[ply - 2];
                move_score += 92 * continuation_table[prev2.piece_type][prev2.to][piece][to] / 100;
                if (ply > 3) {
                    auto prev4 = data.prev_moves[ply - 4];
                    move_score += 78 * continuation_table[prev4.piece_type][prev4.to][piece][to] / 100;
                }
            }
        }

        return move_score;
    }

    int get_capture_score(Piece from_piece, Square to, Piece to_piece) const {
        return capture_table[from_piece][to][to_piece];
    }

    template<Color color>
    void update_correction_history(board& chessboard, const search_data &data, int best_score, int raw_eval, int depth, int material_key) {

        int diff = (best_score - raw_eval) * 256;
        int weight = std::min(128, depth * (depth + 1));

        int &entry = correction_table[color][chessboard.get_pawn_key() % 16384];
        entry = (entry * (256 - weight) + diff * weight) / 256;
        entry = std::clamp(entry, -8'192, 8'192);

        std::uint64_t threat_key = murmur_hash_3(chessboard.get_threats() & chessboard.get_side_occupancy<color>());
        int &threat_entry = threat_correction_table[color][threat_key % 32768];
        threat_entry = (threat_entry * (256 - weight) + diff * weight) / 256;
        threat_entry = std::clamp(threat_entry, -8'192, 8'192);

        int &material_entry = material_correction_table[color][material_key];
        material_entry = (material_entry * (256 - weight) + diff * weight) / 256;
        material_entry = std::clamp(material_entry, -8'192, 8'192);

        auto [wkey, bkey] = chessboard.get_nonpawn_key();
        int &white_nonpawn_entry = nonpawn_correction_table[color][White][wkey % 16384];
        white_nonpawn_entry = (white_nonpawn_entry * (256 - weight) + diff * weight) / 256;
        white_nonpawn_entry = std::clamp(white_nonpawn_entry, -8'192, 8'192);

        int &black_nonpawn_entry = nonpawn_correction_table[color][Black][bkey % 16384];
        black_nonpawn_entry = (black_nonpawn_entry * (256 - weight) + diff * weight) / 256;
        black_nonpawn_entry = std::clamp(black_nonpawn_entry, -8'192, 8'192);

        int &major_entry = major_correction_table[color][chessboard.get_major_key() % 16384];
        major_entry = (major_entry * (256 - weight) + diff * weight) / 256;
        major_entry = std::clamp(major_entry, -8'192, 8'192);

        int &minor_entry = minor_correction_table[color][chessboard.get_minor_key() % 16384];
        minor_entry = (minor_entry * (256 - weight) + diff * weight) / 256;
        minor_entry = std::clamp(minor_entry, -8'192, 8'192);

        if (data.get_ply() > 1) {
            auto prev1 = data.prev_moves[data.get_ply() - 1];
            auto prev2 = data.prev_moves[data.get_ply() - 2];
            int &cont_entry = continuation_correction_table[prev2.piece_type][prev2.to][prev1.piece_type][prev1.to];
            cont_entry = (cont_entry * (256 - weight) + diff * weight) / 256;
            cont_entry = std::clamp(cont_entry, -8'192, 8'192);
        }
    }

    template <Color color>
    std::int16_t correct_eval(const board &chessboard, const search_data &data, int raw_eval, int material_key) {
        if (std::abs(raw_eval) > 8'000) return raw_eval;
        std::uint64_t threat_key = murmur_hash_3(chessboard.get_threats() & chessboard.get_side_occupancy<color>());

        const int entry = correction_table[color][chessboard.get_pawn_key() % 16384];
        const int material_entry = material_correction_table[color][material_key];
        const int threat_entry = threat_correction_table[color][threat_key % 32768];
        const int major_entry = major_correction_table[color][chessboard.get_major_key() % 16384];
        const int minor_entry = minor_correction_table[color][chessboard.get_minor_key() % 16384];

        auto [wkey, bkey] = chessboard.get_nonpawn_key();
        const int nonpawn_entry = nonpawn_correction_table[color][White][wkey % 16384] + nonpawn_correction_table[color][Black][bkey % 16384];

        int cont_entry = 0;
        if (data.get_ply() > 1) {
            auto prev1 = data.prev_moves[data.get_ply() - 1];
            auto prev2 = data.prev_moves[data.get_ply() - 2];
            cont_entry = continuation_correction_table[prev2.piece_type][prev2.to][prev1.piece_type][prev1.to];
        }

        return raw_eval + (entry * pawn_weight.value + mat_weight.value * material_entry + threat_entry * threat_weight.value + nonpawn_entry * nonpawn_weight.value + major_entry * major_weight.value + minor_entry * minor_weight.value + cont_entry * concorrde_weight.value) / (256 * 400);
    }


private:
    std::array<std::array<std::array<std::array<std::array<int, 64>, 64>, 2>, 2>, 2> history_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 2>, 512> material_history_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_table;
    std::array<std::array<std::array<int, 7>, 64>, 6> capture_table;
    std::array<std::array<int, 16384>, 2> correction_table;
    std::array<std::array<std::array<int, 16384>, 2>, 2> nonpawn_correction_table;
    std::array<std::array<int, 16384>, 2> minor_correction_table;
    std::array<std::array<int, 16384>, 2> major_correction_table;
    std::array<std::array<int, 32768>, 2> threat_correction_table;
    std::array<std::array<int, 32768>, 2> material_correction_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_correction_table;

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
