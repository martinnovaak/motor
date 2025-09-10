#include "attacks.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include "types.hpp"

namespace motor::attacks {

    // =========================
    //   Helper Constants
    // =========================
    constexpr int BOARD_SIZE = 64;
    constexpr int RANK_SIZE = 8;
    constexpr std::uint64_t MAIN_DIAGONAL = 0x8040201008040201ULL;
    constexpr std::uint64_t FIRST_FILE = 0x0101010101010101ULL;
    constexpr std::uint8_t HASH_MASK = 0b111111U;

    constexpr std::array<int, 2> RANK_DIRECTIONS = {-1, 1};
    constexpr std::array<int, 2> FILE_DIRECTIONS = {-8, 8};
    constexpr std::array<int, 2> DIAGONAL_DIRECTIONS = {-9, 9};
    constexpr std::array<int, 2> ANTI_DIAGONAL_DIRECTIONS = {-7, 7};

    // =========================
    //   Knight / King Attacks
    // =========================
    template<std::size_t N>
    constexpr std::uint64_t calculateStepAttacks(int square, const std::array<std::array<int, 2>, N> &deltas) {
        const int file = fileOf(square);
        const int rank = rankOf(square);
        std::uint64_t attacks = 0;

        for (const auto &[df, dr]: deltas) {
            const int nf = file + df;
            const int nr = rank + dr;
            if (isValidSquare(nf, nr))
                attacks |= squareToBitboard(squareFrom(nf, nr));
        }
        return attacks;
    }

    constexpr std::array<std::array<int, 2>, 8> KNIGHT_MOVES = {
            {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}}};

    constexpr std::array<std::array<int, 2>, 8> KING_MOVES = {
            {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}}};

    constexpr auto generateKnightAttacks() {
        std::array<std::uint64_t, BOARD_SIZE> table{};
        for (int sq = 0; sq < BOARD_SIZE; ++sq)
            table[sq] = calculateStepAttacks(static_cast<int>(sq), KNIGHT_MOVES);
        return table;
    }

    constexpr auto generateKingAttacks() {
        std::array<std::uint64_t, BOARD_SIZE> table{};
        for (int sq = 0; sq < BOARD_SIZE; ++sq)
            table[sq] = calculateStepAttacks(static_cast<int>(sq), KING_MOVES);
        return table;
    }

    const auto KNIGHT_ATTACKS = generateKnightAttacks();
    const auto KING_ATTACKS = generateKingAttacks();

    // =========================
    //   Slider Attacks
    // =========================
    bool isValidTransition(int from, int to, int direction) {
        if (to < 0 || to >= BOARD_SIZE)
            return false;
        if (std::abs((to % 8) - ((to - direction) % 8)) >= 2)
            return false;
        return true;
    }

    template<typename HashFunc>
    std::uint64_t generateRayOccupancy(std::uint8_t hashKey, int square, const std::array<int, 2> &directions,
                                       HashFunc hashFn) {
        std::uint64_t result = 0;
        for (int dir: directions) {
            int current = square;
            while (true) {
                int next = current + dir;
                if (!isValidTransition(current, next, dir))
                    break;
                const std::uint64_t bit = 1ULL << next;
                result |= bit;
                if (hashFn(square, bit) & hashKey)
                    break;
                current = next;
            }
        }
        return result;
    }

    // ---- Hash functions ----
    std::uint8_t computeRank(int square, std::uint64_t occ) {
        const int rankStart = (square / RANK_SIZE) * RANK_SIZE;
        return static_cast<std::uint8_t>(((occ >> rankStart) >> 1) & HASH_MASK);
    }

    std::uint8_t computeFile(int square, std::uint64_t occ) {
        const int file = square % RANK_SIZE;
        const std::uint64_t fileMask = (occ >> file) & FIRST_FILE;
        return static_cast<std::uint8_t>(((fileMask * MAIN_DIAGONAL) >> 56 >> 1) & HASH_MASK);
    }

    std::uint64_t generateDiagonalMask(int square) {
        return generateRayOccupancy(0, square, {9, -9}, [](int, std::uint64_t) { return 0; });
    }

    std::uint64_t generateAntiDiagonalMask(int square) {
        return generateRayOccupancy(0, square, {7, -7}, [](int, std::uint64_t) { return 0; });
    }

    std::uint8_t computeDiagonal(int square, std::uint64_t occ) {
        const std::uint64_t masked = occ & generateDiagonalMask(square);
        return static_cast<std::uint8_t>(((masked * FIRST_FILE) >> 57) & HASH_MASK);
    }

    std::uint8_t computeAntiDiagonal(int square, std::uint64_t occ) {
        const std::uint64_t masked = occ & generateAntiDiagonalMask(square);
        return static_cast<std::uint8_t>(((masked * FIRST_FILE) >> 57) & HASH_MASK);
    }

    // ---- Precompute ray tables ----
    template<typename HashFunc>
    auto createRayTable(const std::array<int, 2> &directions, HashFunc hashFunc) {
        std::array<std::array<std::uint64_t, BOARD_SIZE>, 64> table{};
        for (int sq = 0; sq < BOARD_SIZE; ++sq)
            for (int key = 0; key < 64; ++key)
                table[sq][key] = generateRayOccupancy(static_cast<std::uint8_t>(key), static_cast<int>(sq), directions,
                                                      hashFunc);
        return table;
    }

    const auto RANK_TABLE = createRayTable(RANK_DIRECTIONS, computeRank);
    const auto FILE_TABLE = createRayTable(FILE_DIRECTIONS, computeFile);
    const auto DIAGONAL_TABLE = createRayTable(DIAGONAL_DIRECTIONS, computeDiagonal);
    const auto ANTI_DIAGONAL_TABLE = createRayTable(ANTI_DIAGONAL_DIRECTIONS, computeAntiDiagonal);

    // =========================
    //   Public API
    // =========================

    std::uint64_t pawnAttacks(Color color, std::uint64_t pawns) {
        if (color == White) {
            std::uint64_t attacks = (pawns << 9) & ~files[FILE_A]; // northwest
            attacks |= (pawns << 7) & ~files[FILE_H]; // northeast
            return attacks;
        } else {
            std::uint64_t attacks = (pawns >> 7) & ~files[FILE_A]; // southeast
            attacks |= (pawns >> 9) & ~files[FILE_H]; // southwest
            return attacks;
        }
    }

    std::uint64_t knightAttacks(int square) { return KNIGHT_ATTACKS[square]; }
    std::uint64_t kingAttacks(int square) { return KING_ATTACKS[square]; }

    std::uint64_t bishopAttacks(int square, std::uint64_t occ) {
        return DIAGONAL_TABLE[square][computeDiagonal(square, occ)] |
               ANTI_DIAGONAL_TABLE[square][computeAntiDiagonal(square, occ)];
    }

    std::uint64_t rookAttacks(int square, std::uint64_t occ) {
        return RANK_TABLE[square][computeRank(square, occ)] | FILE_TABLE[square][computeFile(square, occ)];
    }

    std::uint64_t queenAttacks(int square, std::uint64_t occ) {
        return rookAttacks(square, occ) | bishopAttacks(square, occ);
    }

} // namespace motor::attacks
