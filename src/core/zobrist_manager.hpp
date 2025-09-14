#ifndef MOTOR_ZOBRIST_MANAGER_HPP
#define MOTOR_ZOBRIST_MANAGER_HPP

#include "types.hpp"
#include "zobrist.hpp"

namespace motor {

    // Enum for different key types
    enum KeyType {
        MAIN_KEY, // Full position hash
        PAWN_KEY, // Only pawns
        MINOR_KEY, // Knights and bishops
        MAJOR_KEY, // Rooks and queens
        MATERIAL_KEY // Non-pawn pieces by color
    };

    class ZobristManager {
    public:
        ZobristManager() = default;

        // Update all relevant keys for a piece placement/removal
        void updatePiece(Color color, Piece piece, Square square) {
            // Always update main key
            MainKey.updatePsqtHash(color, piece, square);

            // Update specialized keys for corrhist based on piece type
            if (piece == Pawn) {
                PawnKey.updatePsqtHash(color, piece, square);
            } else {
                MaterialKeys[color].updatePsqtHash(color, piece, square);

                if (piece == Knight || piece == Bishop) {
                    MinorKey.updatePsqtHash(color, piece, square);
                } else if (piece == Rook || piece == Queen) {
                    MajorKey.updatePsqtHash(color, piece, square);
                } else if (piece == King) {
                    MinorKey.updatePsqtHash(color, piece, square);
                    MajorKey.updatePsqtHash(color, piece, square);
                }
            }
        }

        void updateSideToMove() {
            MainKey.updateSideHash();
        }

        void updateCastlingRights(std::uint8_t oldRights, std::uint8_t newRights) {
            if (oldRights != newRights) {
                MainKey.updateCastlingHash(oldRights); // Remove old
                MainKey.updateCastlingHash(newRights); // Add new
            }
        }

        void updateEnPassant(Square oldSquare, Square newSquare) {
            if (oldSquare != newSquare) {
                if (oldSquare != Null_Square) {
                    MainKey.updateEnpassantHash(oldSquare); // Remove old
                }
                if (newSquare != Null_Square) {
                    MainKey.updateEnpassantHash(newSquare); // Add new
                }
            }
        }

        // Get specific key
        [[nodiscard]] std::uint64_t getKey(KeyType type) const {
            switch (type) {
                case MAIN_KEY:
                    return MainKey.getKey();
                case PAWN_KEY:
                    return PawnKey.getKey();
                case MINOR_KEY:
                    return MinorKey.getKey();
                case MAJOR_KEY:
                    return MajorKey.getKey();
                case MATERIAL_KEY:
                    // Combine both colors for material key
                    return MaterialKeys[White].getKey() ^ MaterialKeys[Black].getKey();
                default:
                    return 0;
            }
        }

        // Get material keys separately by color
        [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> getMaterialKeys() const {
            return {MaterialKeys[White].getKey(), MaterialKeys[Black].getKey()};
        }

        bool operator==(const ZobristManager &other) const {
            return MainKey == other.MainKey && PawnKey == other.PawnKey && MinorKey == other.MinorKey &&
                   MajorKey == other.MajorKey && MaterialKeys[White] == other.MaterialKeys[White] &&
                   MaterialKeys[Black] == other.MaterialKeys[Black];
        }

        // Reset all keys
        void reset() {
            MainKey = Zobrist();
            PawnKey = Zobrist();
            MinorKey = Zobrist();
            MajorKey = Zobrist();
            MaterialKeys[White] = Zobrist();
            MaterialKeys[Black] = Zobrist();
        }

        // Batch update for efficiency (when setting up position from FEN)
        void batchUpdateStart() { reset(); }

    private:
        Zobrist MainKey; // Complete position hash
        Zobrist PawnKey; // Pawn structure only
        Zobrist MinorKey; // Knights, bishops, kings
        Zobrist MajorKey; // Rooks, queens, kings
        std::array<Zobrist, 2> MaterialKeys; // Non-pawn pieces by color
    };

} // namespace motor

#endif // MOTOR_ZOBRIST_MANAGER_HPP
