#ifndef HISTORY_HPP
#define HISTORY_HPP

#include <array>
#include <algorithm>
#include <bit>
#include <memory>
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

template <Color color>
std::uint64_t king_shelter_key(const board& chessboard) {
    const Square king = chessboard.get_king_square<color>();
    const int king_file = king % 8;
    const int king_rank = king / 8;
    const int file_direction = king_file < 4 ? 1 : -1;

    auto distance_bucket = [](int distance) {
        if (distance == 0) return 0;
        if (distance == 1) return 1;
        if (distance == 2) return 2;
        if (distance <= 4) return 3;
        return 4;
    };

    const std::uint64_t forward_ranks = [&] {
        if constexpr (color == White) {
            return king_rank == RANK_8
                ? 0ULL
                : full_board << ((king_rank + 1) * 8);
        } else {
            return king_rank == RANK_1
                ? 0ULL
                : (1ULL << (king_rank * 8)) - 1;
        }
    }();

    auto nearest_pawn_distance = [&](std::uint64_t pawns, int file) {
        if (file < FILE_A || file > FILE_H) {
            return 0;
        }

        const std::uint64_t candidates = pawns & files[file] & forward_ranks;
        if (candidates == 0) {
            return 0;
        }

        if constexpr (color == White) {
            return static_cast<int>(std::countr_zero(candidates) / 8) - king_rank;
        } else {
            return king_rank
                 - static_cast<int>((63 - std::countl_zero(candidates)) / 8);
        }
    };

    const std::uint64_t friendly_pawns = chessboard.get_pieces(color, Pawn);
    constexpr Color enemy = color == White ? Black : White;
    const std::uint64_t enemy_pawns = chessboard.get_pieces(enemy, Pawn);

    const int normalized_king_rank = color == White ? king_rank : 7 - king_rank;
    const int normalized_king_file = std::min(king_file, 7 - king_file);
    std::uint64_t key = normalized_king_file | (normalized_king_rank << 2);
    int shift = 5;
    for (int file_delta = -1; file_delta <= 1; ++file_delta) {
        const int file = king_file + file_delta * file_direction;
        key |= static_cast<std::uint64_t>(
                distance_bucket(nearest_pawn_distance(friendly_pawns, file))) << shift;
        shift += 3;
        key |= static_cast<std::uint64_t>(
                distance_bucket(nearest_pawn_distance(enemy_pawns, file))) << shift;
        shift += 3;
    }

    return key;
}

class History {
public:
    History()
            : history_table({}), pawn_history_table({}), continuation_table({}), capture_table({}),
              pawn_correction_table({}), nonpawn_correction_table({}), minor_correction_table({}), major_correction_table({}),
              threat_correction_table({}), king_shelter_correction_table({}),
              continuation_correction_table({}), continuation_correction_table2({}) {}

    void clear() {
        history_table = {};
        pawn_history_table = {};
        continuation_table = {};
        capture_table = {};
        pawn_correction_table = {};
        nonpawn_correction_table = {};
        minor_correction_table = {};
        major_correction_table = {};
        threat_correction_table = {};
        king_shelter_correction_table = {};
        continuation_correction_table = {};
        continuation_correction_table2 = {};
    }

    template <Color color, bool is_root>
    void update(search_data &data, board &chessboard, const chess_move &best_move, move_list &quiets, move_list &captures, int depth) {
        int bonus = history_bonus(depth);
        int penalty = -bonus;

        auto [piece, from, to] = data.prev_moves[data.get_ply()];
        history_move prev = {}, prev2 = {}, prev4 = {};

        const std::uint64_t threats = chessboard.get_threats();
        const std::uint64_t pawn_key = chessboard.get_pawn_key() % 512;

        if (chessboard.is_quiet(best_move)) {
            bool threat_from = (threats & bb(from));
            bool threat_to = (threats & bb(to));
            update_history(history_table[color][threat_from][threat_to][from][to], bonus);
            update_history(pawn_history_table[color][pawn_key][piece][to], bonus);

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
                update_history(pawn_history_table[color][pawn_key][qpiece][qto], penalty);

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
            const auto threat_to = static_cast<bool>(threats & bb(to));
            update_cap_history(capture_table[color][threat_to][piece][to][chessboard.get_piece(to)], bonus);
        }

        for (const auto &capture : captures) {
            const auto cap_to = capture.get_to();
            const auto threat_to = static_cast<bool>(threats & bb(cap_to));
            update_cap_history(capture_table[color][threat_to][chessboard.get_piece(capture.get_from())][cap_to][chessboard.get_piece(cap_to)], penalty);
        }
    }

    template <Color color>
    int get_quiet_score(board &chessboard, const search_data &data, Square from, Square to, Piece piece) const {
        std::uint64_t threats = chessboard.get_threats();
        const bool threat_from = (threats & bb(from));
        const bool threat_to = (threats & bb(to));
        const std::uint64_t pawn_key = chessboard.get_pawn_key() % 512;

        int move_score = history_table[color][threat_from][threat_to][from][to];
        move_score += pawn_history_table[color][pawn_key][piece][to];

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

    template <Color color>
    int get_capture_score(board & chessboard, Piece from_piece, Square to, Piece to_piece) const {
        const auto threat_to = static_cast<bool>(chessboard.get_threats() & bb(to));
        return capture_table[color][threat_to][from_piece][to][to_piece];
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
        const std::uint64_t shelter_key = king_shelter_key<color>(chessboard);
        update_entry(king_shelter_correction_table[color][murmur_hash_3(shelter_key) % CORRECTION_TABLE_SIZE]);

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
        const std::uint64_t shelter_key = king_shelter_key<color>(chessboard);
        const int shelter_entry = king_shelter_correction_table[color][murmur_hash_3(shelter_key) % 16384];

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

        return raw_eval + (pawn_entry * 200 + threat_entry * 100 + nonpawn_entry * 200 + minor_entry * 150 + major_entry * 120 + shelter_entry * 100 + cont_entry * 180 + cont_entry2 * 180) / (256 * 300);
    }


private:
    std::array<std::array<std::array<std::array<std::array<int, 64>, 64>, 2>, 2>, 2> history_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 512>, 2> pawn_history_table;
    std::array<std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7>, 2> continuation_table;
    std::array<std::array<std::array<std::array<std::array<int, 7>, 64>, 6>, 2>, 2> capture_table;
    std::array<std::array<int, 16384>, 2> pawn_correction_table;
    std::array<std::array<std::array<int, 16384>, 2>, 2> nonpawn_correction_table;
    std::array<std::array<int, 16384>, 2> minor_correction_table;
    std::array<std::array<int, 16384>, 2> major_correction_table;
    std::array<std::array<int, 16384>, 2> threat_correction_table;
    std::array<std::array<int, 16384>, 2> king_shelter_correction_table;
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

#endif // HISTORY_HPP
