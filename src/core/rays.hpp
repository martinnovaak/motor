#pragma once

#include <array>
#include <cstdint>

namespace motor::rays {

    namespace detail {
        constexpr int constexpr_abs(int x) { return x < 0 ? -x : x; }

        constexpr int getDirection(int from, int to) {
            const int file_diff = (to % 8) - (from % 8);
            const int rank_diff = (to / 8) - (from / 8);

            if (file_diff == 0 && rank_diff != 0) {
                return (rank_diff > 0) ? 8 : -8;
            } else if (rank_diff == 0 && file_diff != 0) {
                return (file_diff > 0) ? 1 : -1;
            } else if (constexpr_abs(file_diff) == constexpr_abs(rank_diff) && file_diff != 0) {
                if (file_diff > 0 && rank_diff > 0)
                    return 9;
                if (file_diff > 0 && rank_diff < 0)
                    return -7;
                if (file_diff < 0 && rank_diff > 0)
                    return 7;
                if (file_diff < 0 && rank_diff < 0)
                    return -9;
            }

            return 0;
        }

        constexpr bool isKnightMove(int from, int to) {
            const int file_diff = constexpr_abs((to % 8) - (from % 8));
            const int rank_diff = constexpr_abs((to / 8) - (from / 8));
            return (file_diff == 1 && rank_diff == 2) || (file_diff == 2 && rank_diff == 1);
        }

        constexpr std::uint64_t generateRayBetween(int from, int to) {
            const int direction = getDirection(from, to);

            if (direction != 0) {
                std::uint64_t ray = 0;
                int current = from;

                while (current != to) {
                    current += direction;
                    if (current < 0 || current >= 64)
                        break;
                    ray |= (1ULL << current);
                }

                return ray;
            }

            if (isKnightMove(from, to)) {
                return 1ULL << to;
            }

            return 0;
        }

        constexpr auto generateRayBetweenTable() {
            std::array<std::array<std::uint64_t, 64>, 64> table{};

            for (int from = 0; from < 64; ++from) {
                for (int to = 0; to < 64; ++to) {
                    table[from][to] = generateRayBetween(from, to);
                }
            }

            return table;
        }
    } // namespace detail

    constexpr auto RAY_BETWEEN = detail::generateRayBetweenTable();

} // namespace motor::rays
