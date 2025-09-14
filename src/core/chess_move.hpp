#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "types.hpp"

// Square to string conversion table
constexpr std::array<const char *, 64> SQUARE_TO_STRING = {{
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
}};

enum MoveType : std::uint16_t { NORMAL = 0, PROMOTION = 1 << 14, EN_PASSANT = 2 << 14, CASTLING = 3 << 14 };

enum PromotionType : std::uint16_t {
    KNIGHT_PROMOTION = 0,
    BISHOP_PROMOTION = 1 << 12,
    ROOK_PROMOTION = 2 << 12,
    QUEEN_PROMOTION = 3 << 12
};

class ChessMove {
public:
    constexpr ChessMove() : PackedMoveData(0) {}

    constexpr ChessMove(Square From, Square To, MoveType Type = NORMAL, PromotionType Promotion = KNIGHT_PROMOTION) :
        PackedMoveData(static_cast<std::uint16_t>(Type) | From | (To << 6) | Promotion) {}

    // Getters
    [[nodiscard]] constexpr Square getFrom() const {
        return static_cast<Square>(PackedMoveData & 0x3F); // 6 bits
    }

    [[nodiscard]] constexpr Square getTo() const {
        return static_cast<Square>((PackedMoveData >> 6) & 0x3F); // 6 bits
    }

    [[nodiscard]] constexpr MoveType getMoveType() const {
        return static_cast<MoveType>(PackedMoveData & (3 << 14)); // 2 bits
    }

    [[nodiscard]] constexpr Piece getPromotionPiece() const {
        return static_cast<Piece>(((PackedMoveData >> 12) & 3) + Knight); // 2 bits
    }

    [[nodiscard]] constexpr std::uint16_t getValue() const { return PackedMoveData; }

    // Check if this is a null/invalid move
    [[nodiscard]] constexpr bool isNull() const { return PackedMoveData == 0; }

    // Comparison operators
    constexpr bool operator==(const ChessMove &Other) const { return Other.PackedMoveData == PackedMoveData; }

    constexpr bool operator!=(const ChessMove &Other) const { return !(*this == Other); }

    // Convert move to UCI notation string
    [[nodiscard]] std::string toString() const {
        if (isNull()) {
            return "0000"; // Null move representation
        }

        std::string MoveString;
        MoveString += SQUARE_TO_STRING[getFrom()];
        MoveString += SQUARE_TO_STRING[getTo()];

        // Add promotion piece if this is a promotion move
        if (getMoveType() == PROMOTION) {
            switch (getPromotionPiece()) {
                case Knight:
                    MoveString += 'n';
                    break;
                case Bishop:
                    MoveString += 'b';
                    break;
                case Rook:
                    MoveString += 'r';
                    break;
                case Queen:
                    MoveString += 'q';
                    break;
                default:
                    break; // Should not happen
            }
        }

        return MoveString;
    }

    // Create a null move
    static constexpr ChessMove nullMove() { return ChessMove(); }

    // Factory methods for common move types
    static constexpr ChessMove createNormalMove(Square From, Square To) { return ChessMove(From, To, NORMAL); }

    static constexpr ChessMove createPromotionMove(Square From, Square To, Piece PromotionPiece) {
        PromotionType Promotion;
        switch (PromotionPiece) {
            case Knight:
                Promotion = KNIGHT_PROMOTION;
                break;
            case Bishop:
                Promotion = BISHOP_PROMOTION;
                break;
            case Rook:
                Promotion = ROOK_PROMOTION;
                break;
            case Queen:
                Promotion = QUEEN_PROMOTION;
                break;
            default:
                Promotion = QUEEN_PROMOTION; // Default to queen
                break;
        }
        return ChessMove(From, To, PROMOTION, Promotion);
    }

    static constexpr ChessMove createEnPassantMove(Square From, Square To) { return ChessMove(From, To, EN_PASSANT); }

    static constexpr ChessMove createCastlingMove(Square From, Square To) { return ChessMove(From, To, CASTLING); }

private:
    // Move encoding (16 bits total):
    // Bits 0-5:   From square (6 bits, 0-63)
    // Bits 6-11:  To square (6 bits, 0-63)
    // Bits 12-13: Promotion piece (2 bits, for promotion moves)
    // Bits 14-15: Move type (2 bits: 0=normal, 1=promotion, 2=en passant, 3=castling)
    std::uint16_t PackedMoveData;
};

// Utility functions
namespace ChessMoveUtils {

    // Convert square string to Square enum (e.g., "e4" -> E4)
    inline Square squareFromString(const std::string &SquareStr) {
        if (SquareStr.length() < 2) {
            return Square::Null_Square;
        }

        char File = SquareStr[0];
        char Rank = SquareStr[1];

        if ('a' <= File && File <= 'h' && '1' <= Rank && Rank <= '8') {
            return static_cast<Square>((File - 'a') + (Rank - '1') * 8);
        } else if ('A' <= File && File <= 'H' && '1' <= Rank && Rank <= '8') {
            return static_cast<Square>((File - 'A') + (Rank - '1') * 8);
        } else if (SquareStr == "-") {
            return Square::Null_Square;
        } else {
            return Square::Null_Square;
        }
    }

    // Parse move from UCI string (e.g., "e2e4", "e7e8q")
    inline ChessMove parseUCIMove(const std::string &UCIString) {
        if (UCIString.length() < 4) {
            return ChessMove::nullMove();
        }

        Square From = squareFromString(UCIString.substr(0, 2));
        Square To = squareFromString(UCIString.substr(2, 2));

        if (From == Null_Square || To == Null_Square) {
            return ChessMove::nullMove();
        }

        // Check for promotion
        if (UCIString.length() == 5) {
            char PromotionChar = UCIString[4];
            Piece PromotionPiece;

            switch (PromotionChar) {
                case 'n':
                case 'N':
                    PromotionPiece = Knight;
                    break;
                case 'b':
                case 'B':
                    PromotionPiece = Bishop;
                    break;
                case 'r':
                case 'R':
                    PromotionPiece = Rook;
                    break;
                case 'q':
                case 'Q':
                    PromotionPiece = Queen;
                    break;
                default:
                    return ChessMove::nullMove();
            }

            return ChessMove::createPromotionMove(From, To, PromotionPiece);
        }

        return ChessMove::createNormalMove(From, To);
    }

} // namespace ChessMoveUtils
