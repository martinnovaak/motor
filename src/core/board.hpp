#ifndef MOTOR_BOARD_HPP
#define MOTOR_BOARD_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <sstream>
#include <string>
#include <vector>
#include "attacks.hpp"
#include "chess_move.hpp"
#include "rays.hpp"
#include "types.hpp"
#include "zobrist_manager.hpp"

namespace motor {

    // Castling rights bitmask
    enum CastlingRight : std::uint8_t {
        WHITE_KINGSIDE = 1,
        WHITE_QUEENSIDE = 2,
        BLACK_KINGSIDE = 4,
        BLACK_QUEENSIDE = 8,
        WHITE_CASTLING = WHITE_KINGSIDE | WHITE_QUEENSIDE,
        BLACK_CASTLING = BLACK_KINGSIDE | BLACK_QUEENSIDE,
        ALL_CASTLING = WHITE_CASTLING | BLACK_CASTLING
    };

    // Castling mask for updating rights when pieces move
    constexpr std::array<std::uint8_t, 64> CASTLING_MASK = {
            13, 15, 15, 15, 12, 15, 15, 14, // Rank 1
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 2
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 3
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 4
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 5
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 6
            15, 15, 15, 15, 15, 15, 15, 15, // Rank 7
            7,  15, 15, 15, 3,  15, 15, 11 // Rank 8
    };

    struct BoardInfo {
        std::uint8_t CastlingRights = 0;
        Square EnpassantSquare = Null_Square;
        std::uint8_t FiftyMoveCounter = 0;
        Piece CapturedPiece = Null_Piece;
        ChessMove LastMove = ChessMove::nullMove();
        ZobristManager Keys;
        std::uint64_t Threats = 0;
        std::uint64_t Checkers = 0;
        std::uint64_t CheckMask = 0;
        std::uint64_t PinDiagonal = 0;
        std::uint64_t PinOrthogonal = 0;
    };

    class Board {
    public:
        static constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        explicit Board(const std::string &fen = std::string(STARTING_FEN)) {
            reset();
            setFromFen(fen);
        }

        // Board state access
        [[nodiscard]] Piece getPiece(Square square) const { return PieceArray[square]; }

        [[nodiscard]] Color getSideToMove() const { return SideToMove; }

        [[nodiscard]] std::uint64_t getOccupancy() const { return AllOccupancy; }

        [[nodiscard]] std::uint64_t getSideOccupancy(Color color) const { return SideOccupancy[color]; }

        [[nodiscard]] std::uint64_t getPieces(Color color, Piece piece) const { return Bitboards[color][piece]; }

        [[nodiscard]] Square getKingSquare(Color color) const {
            return static_cast<Square>(std::countr_zero(Bitboards[color][King]));
        }

        [[nodiscard]] Square getKingSquare() const { return getKingSquare(SideToMove); }

        // Game state queries
        [[nodiscard]] bool isInCheck() const { return getCurrentState().Checkers != 0; }

        [[nodiscard]] Square getEnpassantSquare() const { return getCurrentState().EnpassantSquare; }

        [[nodiscard]] bool canCastle(CastlingRight right) const {
            return (getCurrentState().CastlingRights & right) != 0;
        }

        [[nodiscard]] std::uint64_t getThreats() const { return getCurrentState().Threats; }

        [[nodiscard]] std::uint64_t getCheckers() const { return getCurrentState().Checkers; }

        [[nodiscard]] std::uint64_t getCheckMask() const { return getCurrentState().CheckMask; }

        [[nodiscard]] std::uint64_t getPinDiagonal() const { return getCurrentState().PinDiagonal; }

        [[nodiscard]] std::uint64_t getPinOrthogonal() const { return getCurrentState().PinOrthogonal; }

        // Hash keys
        [[nodiscard]] std::uint64_t getHashKey() const { return getCurrentState().Keys.getKey(MAIN_KEY); }

        [[nodiscard]] std::uint64_t getPawnKey() const { return getCurrentState().Keys.getKey(PAWN_KEY); }

        // Material keys
        [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> getNonPawnKey() const {
            return getCurrentState().Keys.getMaterialKeys();
        }

        [[nodiscard]] std::uint64_t getMajorKey() const { return getCurrentState().Keys.getKey(MAJOR_KEY); }

        [[nodiscard]] std::uint64_t getMinorKey() const { return getCurrentState().Keys.getKey(MINOR_KEY); }

        [[nodiscard]] std::uint64_t getMaterialKey() const { return getCurrentState().Keys.getKey(MATERIAL_KEY); }

        // Move validation
        [[nodiscard]] bool isQuietMove(const ChessMove &move) const {
            return move.getMoveType() != PROMOTION && move.getMoveType() != EN_PASSANT &&
                   getPiece(move.getTo()) == Null_Piece;
        }

        [[nodiscard]] bool isCaptureMove(const ChessMove &move) const { return getPiece(move.getTo()) != Null_Piece; }

        // Draw detection
        [[nodiscard]] bool isDraw(int ply) const {
            const auto &currentState = getCurrentState();
            if (currentState.FiftyMoveCounter < 4)
                return false;
            if (currentState.FiftyMoveCounter >= 100)
                return true;

            const int currentIndex = static_cast<int>(CurrentStateIndex);
            const int endIndex = std::max(0, currentIndex - static_cast<int>(currentState.FiftyMoveCounter));

            int repetitions = 0;
            for (int i = currentIndex - 4; i >= endIndex; i -= 2) {
                if (i >= 0 && i < static_cast<int>(StateHistory.size()) && StateHistory[i].Keys == currentState.Keys) {
                    if (i > currentIndex - ply)
                        return true;
                    if (++repetitions >= 2)
                        return true;
                }
            }
            return false;
        }

        // Move making/unmaking
        void makeMove(const ChessMove &move);
        void unmakeMove(const ChessMove &move);
        void makeNullMove();
        void unmakeNullMove();

        // FEN handling
        void setFromFen(const std::string &fen);
        [[nodiscard]] std::string toFen() const;

        // Utility methods
        [[nodiscard]] std::uint64_t getOrthogonalPieces(Color color) const {
            return Bitboards[color][Rook] | Bitboards[color][Queen];
        }

        [[nodiscard]] std::uint64_t getDiagonalPieces(Color color) const {
            return Bitboards[color][Bishop] | Bitboards[color][Queen];
        }

        [[nodiscard]] Piece getCapturedPiece() const { return getCurrentState().CapturedPiece; }

        [[nodiscard]] int getMoveCount() const { return static_cast<int>((CurrentStateIndex + 1) / 2); }

        [[nodiscard]] bool isPawnEndgame() const {
            return SideOccupancy[SideToMove] == (Bitboards[SideToMove][Pawn] | Bitboards[SideToMove][King]);
        }

        void resetFiftyMoveCounter() { getCurrentStateRef().FiftyMoveCounter = 0; }

        void setEnpassantSquare(Square square) {
            auto &currentState = getCurrentStateRef();
            currentState.Keys.updateEnPassant(currentState.EnpassantSquare, square);
            currentState.EnpassantSquare = square;
        }

    private:
        // Board representation
        std::array<Piece, 64> PieceArray;
        std::array<std::array<std::uint64_t, 6>, 2> Bitboards; // [Color][Piece]
        std::array<std::uint64_t, 2> SideOccupancy;
        std::uint64_t AllOccupancy;

        // Game state
        Color SideToMove;
        std::vector<BoardInfo> StateHistory;
        std::size_t CurrentStateIndex;

        // Helper methods
        void reset();
        void updateOccupancy();
        void updateZobristKeys(Color color, Piece piece, Square square);

        void calculateThreats(Color us);
        void calculateCheckers(Color us);
        void calculatePins(Color us);
        void updateBoardState();

        [[nodiscard]] std::uint64_t getAttackers(Color us, Square square) const;
        [[nodiscard]] std::uint64_t getAttackers(Color us, Square square, std::uint64_t occupancy) const;

        // Piece manipulation
        void setPiece(Color color, Piece piece, Square square);
        void removePiece(Color color, Piece piece, Square square);
        void movePiece(Color color, Piece piece, Square from, Square to);

        // Castling helpers
        void updateCastlingRights(Square square);

        // State management
        void pushState(const ChessMove &move, Piece capturedPiece);
        void popState();

        [[nodiscard]] const BoardInfo &getCurrentState() const { return StateHistory[CurrentStateIndex]; }

        [[nodiscard]] BoardInfo &getCurrentStateRef() { return StateHistory[CurrentStateIndex]; }

        [[nodiscard]] std::size_t getCurrentStateIndex() const { return CurrentStateIndex; }

        // FEN parsing helpers
        [[nodiscard]] std::pair<Color, Piece> parseCharToPiece(char c) const;
        [[nodiscard]] std::uint8_t parseCastlingRights(const std::string &castling) const;
        [[nodiscard]] Square parseEnpassantSquare(const std::string &enpassant) const;
    };

} // namespace motor

#endif // MOTOR_BOARD_HPP
