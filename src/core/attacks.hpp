#pragma once

#include <array>
#include <cstdint>
#include "types.hpp"

namespace motor::attacks {

    // int manipulation utilities
    constexpr int fileOf(int square) { return square % 8; }
    constexpr int rankOf(int square) { return square / 8; }
    constexpr bool isValidSquare(int file, int rank) { return file >= 0 && file <= 7 && rank >= 0 && rank <= 7; }
    constexpr int squareFrom(int file, int rank) { return rank * 8 + file; }
    constexpr std::uint64_t squareToBitboard(int square) { return static_cast<std::uint64_t>(1) << square; }

    // Function prototypes
    std::uint64_t pawnAttacks(Color color, std::uint64_t pawns);
    std::uint64_t knightAttacks(int square);
    std::uint64_t kingAttacks(int square);
    std::uint64_t bishopAttacks(int square, std::uint64_t occupancy);
    std::uint64_t rookAttacks(int square, std::uint64_t occupancy);
    std::uint64_t queenAttacks(int square, std::uint64_t occupancy);

} // namespace motor::attacks
