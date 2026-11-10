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
#ifdef MOTOR_CORRHIST_PROFILE
#include "../corrhist_similarity_profile.hpp"
#endif

auto murmur_hash_3(std::uint64_t key) -> std::uint64_t {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

#ifdef MOTOR_CORRHIST_PROFILE
constexpr auto make_king_outer_rings() {
    std::array<std::uint64_t, 64> rings = {};
    for (int king = 0; king < 64; king++) {
        const int king_file = king % 8;
        const int king_rank = king / 8;
        for (int rank_delta = -2; rank_delta <= 2; rank_delta++) {
            for (int file_delta = -2; file_delta <= 2; file_delta++) {
                const bool outer_rank = rank_delta == -2 || rank_delta == 2;
                const bool outer_file = file_delta == -2 || file_delta == 2;
                if (!outer_rank && !outer_file) continue;
                const int file = king_file + file_delta;
                const int rank = king_rank + rank_delta;
                if (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                    rings[king] |= 1ULL << (rank * 8 + file);
                }
            }
        }
    }
    return rings;
}

constexpr auto king_outer_rings = make_king_outer_rings();

template <Color color>
std::uint64_t king_shelter_profile_key(const board &chessboard) {
    const Square king = chessboard.get_king_square<color>();
    const int king_file = king % 8;
    const int king_rank = king / 8;
    const int file_direction = king_file < 4 ? 1 : -1;
    const std::uint64_t forward_ranks = [&] {
        if constexpr (color == White) {
            return king_rank == 7 ? 0ULL : full_board << ((king_rank + 1) * 8);
        } else {
            return king_rank == 0 ? 0ULL : (1ULL << (king_rank * 8)) - 1;
        }
    }();
    auto distance_bucket = [](int distance) {
        if (distance <= 2) return distance;
        if (distance <= 4) return 3;
        return 4;
    };
    auto nearest_distance = [&](std::uint64_t pawns, int file) {
        if (file < 0 || file >= 8) return 0;
        const std::uint64_t candidates = pawns & files[file] & forward_ranks;
        if (candidates == 0) return 0;
        if constexpr (color == White) {
            return static_cast<int>(std::countr_zero(candidates) / 8) - king_rank;
        } else {
            return king_rank - static_cast<int>((63 - std::countl_zero(candidates)) / 8);
        }
    };

    constexpr Color enemy = color == White ? Black : White;
    const std::uint64_t friendly_pawns = chessboard.get_pieces(color, Pawn);
    const std::uint64_t enemy_pawns = chessboard.get_pieces(enemy, Pawn);
    const int normalized_rank = color == White ? king_rank : 7 - king_rank;
    std::uint64_t key = std::min(king_file, 7 - king_file) | (normalized_rank << 2);
    int shift = 5;
    for (int delta = -1; delta <= 1; delta++) {
        const int file = king_file + delta * file_direction;
        key |= static_cast<std::uint64_t>(
                distance_bucket(nearest_distance(friendly_pawns, file))) << shift;
        shift += 3;
        key |= static_cast<std::uint64_t>(
                distance_bucket(nearest_distance(enemy_pawns, file))) << shift;
        shift += 3;
    }
    return key;
}

template <Color color>
std::uint64_t king_pressure_profile_key(const board &chessboard) {
    constexpr Color enemy = color == White ? Black : White;
    const Square king = chessboard.get_king_square<color>();
    const int king_file = king % 8;
    const int king_rank = king / 8;
    const std::uint64_t threats = chessboard.get_threats();
    const int inner = std::min(7, static_cast<int>(popcount(threats & KING_ATTACKS[king])));
    const int outer = std::min(7, static_cast<int>(popcount(threats & king_outer_rings[king])));
    const int occupied = std::min(3, static_cast<int>(popcount(
            threats & KING_ATTACKS[king] & chessboard.get_side_occupancy<color>())));
    const int normalized_rank = color == White ? king_rank : 7 - king_rank;
    return std::min(king_file, 7 - king_file)
         | (normalized_rank << 2)
         | (inner << 5)
         | (outer << 8)
         | (occupied << 11)
         | (static_cast<std::uint64_t>(chessboard.get_pieces(enemy, Queen) != 0) << 13);
}

template <Color side>
std::uint64_t profile_passed_pawns(std::uint64_t pawns, std::uint64_t enemy_pawns) {
    std::uint64_t frontier = enemy_pawns
            | ((enemy_pawns & ~files[FILE_H]) << 1)
            | ((enemy_pawns & ~files[FILE_A]) >> 1);
    std::uint64_t front_span = 0;
    for (int distance = 0; distance < 7; distance++) {
        if constexpr (side == White) frontier >>= 8;
        else frontier <<= 8;
        front_span |= frontier;
    }
    return pawns & ~front_span;
}

template <Color color>
std::uint64_t passed_pawn_structure_profile_key(const board &chessboard) {
    constexpr Color enemy = color == White ? Black : White;
    std::uint64_t ours = profile_passed_pawns<color>(
            chessboard.get_pieces(color, Pawn), chessboard.get_pieces(enemy, Pawn));
    std::uint64_t theirs = profile_passed_pawns<enemy>(
            chessboard.get_pieces(enemy, Pawn), chessboard.get_pieces(color, Pawn));
    if constexpr (color == Black) {
        ours = __builtin_bswap64(ours);
        theirs = __builtin_bswap64(theirs);
    }
    return murmur_hash_3(ours ^ std::rotl(theirs, 23) ^ 0x5d6e8f102143769aULL);
}

template <Color color>
std::uint64_t piece_square_region_profile_key(const board &chessboard) {
    constexpr Color enemy = color == White ? Black : White;
    std::uint64_t key = 0x243f6a8885a308d3ULL;
    for (int relative_side = 0; relative_side < 2; relative_side++) {
        const Color side = relative_side == 0 ? color : enemy;
        for (int piece = Knight; piece <= Queen; piece++) {
            std::uint64_t pieces = chessboard.get_pieces(side, piece);
            key = murmur_hash_3(key ^ static_cast<std::uint64_t>(
                    1 + relative_side * 8 + piece));
            while (pieces) {
                int square = pop_lsb(pieces);
                if constexpr (color == Black) square ^= 56;
                const int region = (square / 16) * 4 + (square % 8) / 2;
                const std::uint64_t token = 1 + region + 16 * piece + 96 * relative_side;
                key = murmur_hash_3(key ^ token * 0x9e3779b97f4a7c15ULL);
            }
        }
    }
    return key;
}
#endif

class History {
public:
    History()
            : history_table({}), pawn_history_table({}), continuation_table({}), capture_table({}),
              pawn_correction_table({}), nonpawn_correction_table({}), minor_correction_table({}), major_correction_table({}),
              threat_correction_table({}), continuation_correction_table({}), continuation_correction_table2({}) {}

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

        if (data.get_ply() > 1) {
            auto prev1 = data.prev_moves[data.get_ply() - 1];
            auto prev2 = data.prev_moves[data.get_ply() - 2];
            update_entry(continuation_correction_table[prev2.piece_type][prev2.to][prev1.piece_type][prev1.to]);

            if (data.get_ply() > 2) {
                auto prev3 = data.prev_moves[data.get_ply() - 3];
                update_entry(continuation_correction_table2[prev3.piece_type][prev3.to][prev1.piece_type][prev1.to]);
            }
        }
#ifdef MOTOR_CORRHIST_PROFILE
        profile_correction_history<color>(chessboard, data, true);
#endif
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

#ifdef MOTOR_CORRHIST_PROFILE
        profile_correction_history<color>(chessboard, data, false);
#endif

        return raw_eval + (pawn_entry * 200 + threat_entry * 100 + nonpawn_entry * 200 + minor_entry * 150 + major_entry * 120 + cont_entry * 180 + cont_entry2 * 180) / (256 * 300);
    }


private:
#ifdef MOTOR_CORRHIST_PROFILE
    static std::size_t continuation_profile_index(
            const history_move &older, const history_move &newer) {
        return (((static_cast<std::size_t>(older.piece_type) * 64 + older.to) * 7
               + newer.piece_type) * 64 + newer.to);
    }

    static std::uint64_t continuation_profile_key(
            const history_move &older, const history_move &newer) {
        return static_cast<std::uint64_t>(older.piece_type)
             | (static_cast<std::uint64_t>(older.to) << 3)
             | (static_cast<std::uint64_t>(newer.piece_type) << 9)
             | (static_cast<std::uint64_t>(newer.to) << 12);
    }

    template <Color color>
    void profile_correction_history(
            const board &chessboard, const search_data &data, bool update) {
        if (!update && !corrhist_profiler.begin_lookup()) return;
        if (update && !corrhist_profiler.is_enabled()) return;

        auto observe = [&](corrhist_profile::Kind kind, std::uint64_t key,
                           std::size_t table_index) {
            if (update) corrhist_profiler.update(kind, key, table_index);
            else corrhist_profiler.lookup(kind, key, table_index);
        };
        constexpr std::size_t table_size = 16'384;
        constexpr std::size_t color_offset = static_cast<std::size_t>(color) * table_size;

        const std::uint64_t pawn_key = chessboard.get_pawn_key();
        const std::uint64_t threat_key = murmur_hash_3(
                chessboard.get_threats() & chessboard.get_side_occupancy<color>());
        const std::uint64_t minor_key = chessboard.get_minor_key();
        const std::uint64_t major_key = chessboard.get_major_key();
        observe(corrhist_profile::Kind::Pawn, pawn_key,
                color_offset + pawn_key % table_size);
        observe(corrhist_profile::Kind::Threat, threat_key,
                color_offset + threat_key % table_size);
        observe(corrhist_profile::Kind::Minor, minor_key,
                color_offset + minor_key % table_size);
        observe(corrhist_profile::Kind::Major, major_key,
                color_offset + major_key % table_size);

        auto [white_nonpawn, black_nonpawn] = chessboard.get_nonpawn_key();
        const std::size_t nonpawn_base = static_cast<std::size_t>(color) * 2 * table_size;
        observe(corrhist_profile::Kind::Nonpawn, white_nonpawn,
                nonpawn_base + white_nonpawn % table_size);
        observe(corrhist_profile::Kind::Nonpawn, black_nonpawn,
                nonpawn_base + table_size + black_nonpawn % table_size);

        if (data.get_ply() > 1) {
            const history_move newer = data.prev_moves[data.get_ply() - 1];
            const history_move older = data.prev_moves[data.get_ply() - 2];
            observe(corrhist_profile::Kind::Continuation1,
                    continuation_profile_key(older, newer),
                    continuation_profile_index(older, newer));
            if (data.get_ply() > 2) {
                const history_move oldest = data.prev_moves[data.get_ply() - 3];
                observe(corrhist_profile::Kind::Continuation2,
                        continuation_profile_key(oldest, newer),
                        continuation_profile_index(oldest, newer));
            } else {
                corrhist_profiler.unavailable(
                        corrhist_profile::Kind::Continuation2, update);
            }
        } else {
            corrhist_profiler.unavailable(
                    corrhist_profile::Kind::Continuation1, update);
            corrhist_profiler.unavailable(
                    corrhist_profile::Kind::Continuation2, update);
        }

        auto observe_candidate = [&](corrhist_profile::Kind kind,
                                     std::uint64_t key) {
            observe(kind, key, color_offset + murmur_hash_3(key) % table_size);
        };
        observe_candidate(corrhist_profile::Kind::KingShelter,
                          king_shelter_profile_key<color>(chessboard));
        observe_candidate(corrhist_profile::Kind::PassedPawnStructure,
                          passed_pawn_structure_profile_key<color>(chessboard));
        observe_candidate(corrhist_profile::Kind::KingPressure,
                          king_pressure_profile_key<color>(chessboard));
        observe_candidate(corrhist_profile::Kind::PieceSquareRegion,
                          piece_square_region_profile_key<color>(chessboard));
    }
#endif

    std::array<std::array<std::array<std::array<std::array<int, 64>, 64>, 2>, 2>, 2> history_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 512>, 2> pawn_history_table;
    std::array<std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7>, 2> continuation_table;
    std::array<std::array<std::array<std::array<std::array<int, 7>, 64>, 6>, 2>, 2> capture_table;
    std::array<std::array<int, 16384>, 2> pawn_correction_table;
    std::array<std::array<std::array<int, 16384>, 2>, 2> nonpawn_correction_table;
    std::array<std::array<int, 16384>, 2> minor_correction_table;
    std::array<std::array<int, 16384>, 2> major_correction_table;
    std::array<std::array<int, 16384>, 2> threat_correction_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_correction_table;
    std::array<std::array<std::array<std::array<int, 64>, 7>, 64>, 7> continuation_correction_table2;
#ifdef MOTOR_CORRHIST_PROFILE
    corrhist_profile::Profiler corrhist_profiler;
#endif

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
