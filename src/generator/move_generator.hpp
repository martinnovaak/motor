#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include "attacks.hpp"
#include "board.hpp"
#include "chess_move.hpp"
#include "types.hpp"

namespace motor {

    class MoveList {
    private:
        std::array<ChessMove, 256> moves;
        std::array<std::int32_t, 256> scores;
        std::uint8_t count;

    public:
        MoveList() : moves{}, scores{}, count{0} {}

        [[nodiscard]] std::uint8_t size() const { return count; }

        [[nodiscard]] bool empty() const { return count == 0; }

        void push_back(const ChessMove &move) {
            moves[count] = move;
            count++;
        }

        void push_back(ChessMove &&move) {
            moves[count] = std::move(move);
            count++;
        }

        void clear() { count = 0; }

        // Partial insertion sort - get next best move
        ChessMove &getNextMove(std::uint8_t index) {
            std::uint8_t best = index;
            for (std::uint8_t i = index + 1; i < count; i++) {
                if (scores[i] > scores[best]) {
                    best = i;
                }
            }
            std::swap(moves[index], moves[best]);
            std::swap(scores[index], scores[best]);
            return moves[index];
        }

        // Access move scores
        std::int32_t &operator[](int index) { return scores[index]; }
        std::int32_t getMoveScore(int index) const { return scores[index]; }

        // Iterator support
        using iterator = ChessMove *;
        using const_iterator = const ChessMove *;

        iterator begin() { return &moves[0]; }
        const_iterator begin() const { return &moves[0]; }
        iterator end() { return &moves[count]; }
        const_iterator end() const { return &moves[count]; }

        // Access moves directly
        const ChessMove &getMove(std::uint8_t index) const { return moves[index]; }
        ChessMove &getMove(std::uint8_t index) { return moves[index]; }
    };

    class MoveGenerator {
    public:
        // Generate all legal moves
        static void generateAllMoves(const Board &board, MoveList &moves);

        // Generate only captures
        static void generateCaptures(const Board &board, MoveList &moves);

        // Generate only quiet moves
        static void generateQuietMoves(const Board &board, MoveList &moves);

    private:
        // Core move generation functions
        static void generatePawnMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures, MoveList &moves);
        static void generateKnightMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                        MoveList &moves);
        static void generateBishopMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                        MoveList &moves);
        static void generateRookMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures, MoveList &moves);
        static void generateQueenMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                       MoveList &moves);
        static void generateKingMoves(const Board &board, Color side, bool onlyCaptures, MoveList &moves);
        static void generateCastlingMoves(const Board &board, Color side, MoveList &moves);

        // Specialized pawn move functions
        static void generatePawnPushes(const Board &board, Color side, bool inCheck, MoveList &moves);
        static void generatePawnCaptures(const Board &board, Color side, bool inCheck, MoveList &moves);
        static void generatePromotions(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                       MoveList &moves);
        static void generateEnPassant(const Board &board, Color side, bool inCheck, MoveList &moves);

        // Helper functions
        static std::uint64_t getPawnPushTargets(const Board &board, Color side, std::uint64_t pawns);
        static std::uint64_t getPawnCaptureTargets(const Board &board, Color side, std::uint64_t pawns);
        static bool isSquareSafe(const Board &board, Color side, Square square);

        // Direction shifts
        static std::uint64_t shiftNorth(std::uint64_t bb) { return bb << 8; }
        static std::uint64_t shiftSouth(std::uint64_t bb) { return bb >> 8; }
        static std::uint64_t shiftNorthEast(std::uint64_t bb) { return (bb << 9) & ~files[FILE_A]; }
        static std::uint64_t shiftNorthWest(std::uint64_t bb) { return (bb << 7) & ~files[FILE_H]; }
        static std::uint64_t shiftSouthEast(std::uint64_t bb) { return (bb >> 7) & ~files[FILE_A]; }
        static std::uint64_t shiftSouthWest(std::uint64_t bb) { return (bb >> 9) & ~files[FILE_H]; }

        // Pop least significant bit and return the square
        static Square popLSB(std::uint64_t &bb) {
            Square sq = static_cast<Square>(std::countr_zero(bb));
            bb &= bb - 1;
            return sq;
        }
    };

    // Castling path masks
    constexpr std::uint64_t WHITE_KINGSIDE_PATH = (1ULL << F1) | (1ULL << G1);
    constexpr std::uint64_t WHITE_QUEENSIDE_PATH = (1ULL << B1) | (1ULL << C1) | (1ULL << D1);
    constexpr std::uint64_t BLACK_KINGSIDE_PATH = (1ULL << F8) | (1ULL << G8);
    constexpr std::uint64_t BLACK_QUEENSIDE_PATH = (1ULL << B8) | (1ULL << C8) | (1ULL << D8);

    constexpr std::uint64_t WHITE_KINGSIDE_KING_PATH = (1ULL << E1) | (1ULL << F1) | (1ULL << G1);
    constexpr std::uint64_t WHITE_QUEENSIDE_KING_PATH = (1ULL << E1) | (1ULL << D1) | (1ULL << C1);
    constexpr std::uint64_t BLACK_KINGSIDE_KING_PATH = (1ULL << E8) | (1ULL << F8) | (1ULL << G8);
    constexpr std::uint64_t BLACK_QUEENSIDE_KING_PATH = (1ULL << E8) | (1ULL << D8) | (1ULL << C8);

} // namespace motor
