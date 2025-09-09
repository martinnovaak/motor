#define CATCH_CONFIG_MAIN
#include <random>
#include <unordered_set>
#include <vector>
#include "../src/core/zobrist.hpp"
#include "catch2/catch_all.hpp"

// Test helper to create a zobrist hash from position components
std::uint64_t createHash(const std::vector<std::tuple<Color, Piece, Square>> &pieces, std::uint8_t castlingRights = 0,
                         Square enpassantSquare = Null_Square, bool sideToMove = false) {
    Zobrist hash;

    for (const auto &[color, piece, square]: pieces) {
        hash.updatePsqtHash(color, piece, square);
    }

    if (castlingRights != 0) {
        hash.updateCastlingHash(castlingRights);
    }

    if (enpassantSquare != Null_Square) {
        hash.updateEnpassantHash(enpassantSquare);
    }

    if (sideToMove) {
        hash.updateSideHash();
    }

    return hash.getKey();
}

TEST_CASE("Zobrist keys are unique", "[zobrist][uniqueness]") {

    SECTION("All piece-square keys are unique") {
        std::unordered_set<std::uint64_t> pieceSquareKeys;

        for (int color = White; color <= Black; ++color) {
            for (int piece = Pawn; piece < Null_Piece; ++piece) {
                for (int square = A1; square < Null_Square; ++square) {
                    std::uint64_t key = zobrist_keys::PSQT_KEYS[color][piece][square];

                    REQUIRE(key != 0);
                    REQUIRE(pieceSquareKeys.find(key) == pieceSquareKeys.end());
                    pieceSquareKeys.insert(key);
                }
            }
        }

        REQUIRE(pieceSquareKeys.size() == 2 * 6 * 64); // 768 unique keys
    }

    SECTION("All castling keys are unique") {
        std::unordered_set<std::uint64_t> castlingKeys;

        REQUIRE(zobrist_keys::CASTLING_KEYS[0] == 0); // No castling rights = 0

        for (int i = 1; i < 16; ++i) {
            std::uint64_t key = zobrist_keys::CASTLING_KEYS[i];
            REQUIRE(key != 0);
            REQUIRE(castlingKeys.find(key) == castlingKeys.end());
            castlingKeys.insert(key);
        }

        REQUIRE(castlingKeys.size() == 15);
    }

    SECTION("All en passant keys are unique") {
        std::unordered_set<std::uint64_t> enpassantKeys;

        for (int i = A1; i < Null_Square; ++i) {
            std::uint64_t key = zobrist_keys::ENPASSANT_KEYS[i];
            REQUIRE(key != 0);
            REQUIRE(enpassantKeys.find(key) == enpassantKeys.end());
            enpassantKeys.insert(key);
        }

        REQUIRE(zobrist_keys::ENPASSANT_KEYS[Null_Square] == 0); // No en passant = 0
        REQUIRE(enpassantKeys.size() == 64);
    }

    SECTION("Side key is unique from all other keys") {
        std::unordered_set<std::uint64_t> allKeys;

        // Collect all piece-square keys
        for (int color = White; color <= Black; ++color) {
            for (int piece = Pawn; piece < Null_Piece; ++piece) {
                for (int square = A1; square < Null_Square; ++square) {
                    allKeys.insert(zobrist_keys::PSQT_KEYS[color][piece][square]);
                }
            }
        }

        // Collect castling keys (except 0)
        for (int i = 1; i < 16; ++i) {
            allKeys.insert(zobrist_keys::CASTLING_KEYS[i]);
        }

        // Collect en passant keys (except Null_Square)
        for (int i = A1; i < Null_Square; ++i) {
            allKeys.insert(zobrist_keys::ENPASSANT_KEYS[i]);
        }

        // Side key should be unique
        std::uint64_t sideKey = zobrist_keys::SIDE_KEY;
        REQUIRE(sideKey != 0);
        REQUIRE(allKeys.find(sideKey) == allKeys.end());
    }
}

TEST_CASE("Position hashes have minimal collisions", "[zobrist][collisions]") {

    SECTION("Single piece positions are unique") {
        std::unordered_set<std::uint64_t> positionHashes;

        for (int color = White; color <= Black; ++color) {
            for (int piece = Pawn; piece < Null_Piece; ++piece) {
                for (int square = A1; square < Null_Square; ++square) {
                    std::uint64_t hash = createHash(
                            {{static_cast<Color>(color), static_cast<Piece>(piece), static_cast<Square>(square)}});

                    REQUIRE(positionHashes.find(hash) == positionHashes.end());
                    positionHashes.insert(hash);
                }
            }
        }

        REQUIRE(positionHashes.size() == 2 * 6 * 64);
    }

    SECTION("Castling and side combinations are unique") {
        std::unordered_set<std::uint64_t> positionHashes;

        for (int castling = 0; castling < 16; ++castling) {
            for (int side = 0; side < 2; ++side) {
                std::uint64_t hash = createHash({}, castling, Null_Square, side == 1);

                REQUIRE(positionHashes.find(hash) == positionHashes.end());
                positionHashes.insert(hash);
            }
        }

        REQUIRE(positionHashes.size() == 32);
    }
}

TEST_CASE("Complex position collision rate", "[zobrist][stress]") {
    std::unordered_set<std::uint64_t> complexHashes;
    int collisionCount = 0;
    const int numTests = 50000;

    std::mt19937_64 rng(12345);

    for (int test = 0; test < numTests; ++test) {
        std::vector<std::tuple<Color, Piece, Square>> pieces;

        // Add 8-16 random pieces
        int numPieces = 8 + (rng() % 9);
        std::unordered_set<Square> usedSquares;

        for (int i = 0; i < numPieces; ++i) {
            Square square;
            do {
                square = static_cast<Square>(rng() % Null_Square);
            } while (usedSquares.find(square) != usedSquares.end());

            usedSquares.insert(square);

            Color color = static_cast<Color>(rng() % 2);
            Piece piece = static_cast<Piece>(rng() % Null_Piece);
            pieces.emplace_back(color, piece, square);
        }

        std::uint8_t castling = static_cast<std::uint8_t>(rng() % 16);
        Square enpassant = (rng() % 10 == 0) ? static_cast<Square>(rng() % Null_Square) : Null_Square;
        bool side = rng() % 2;

        std::uint64_t hash = createHash(pieces, castling, enpassant, side);

        if (complexHashes.find(hash) != complexHashes.end()) {
            collisionCount++;
        }
        complexHashes.insert(hash);
    }

    INFO("Collision rate: " << (collisionCount * 100.0 / numTests) << "%");
    INFO("Total collisions: " << collisionCount << " out of " << numTests);

    REQUIRE(collisionCount < 10);
    REQUIRE(complexHashes.size() >= numTests - 10);
}

TEST_CASE("Zobrist XOR properties", "[zobrist][properties]") {

    SECTION("XOR self-inverse property") {
        Zobrist hash1;

        hash1.updatePsqtHash(White, Pawn, A1);
        hash1.updatePsqtHash(White, Pawn, A1); // XOR twice should cancel out

        REQUIRE(hash1.getKey() == 0);
    }

    SECTION("XOR commutativity") {
        Zobrist hashA, hashB;

        // Apply operations in one order
        hashA.updatePsqtHash(White, Pawn, A1);
        hashA.updatePsqtHash(Black, Knight, B1);
        hashA.updateSideHash();
        hashA.updateCastlingHash(5);

        // Apply same operations in different order
        hashB.updateCastlingHash(5);
        hashB.updateSideHash();
        hashB.updatePsqtHash(Black, Knight, B1);
        hashB.updatePsqtHash(White, Pawn, A1);

        REQUIRE(hashA.getKey() == hashB.getKey());
    }

    SECTION("Different operations produce different hashes") {
        Zobrist hash1, hash2, hash3;

        hash1.updatePsqtHash(White, Pawn, A1);
        hash2.updatePsqtHash(White, Pawn, A2); // Different square
        hash3.updatePsqtHash(Black, Pawn, A1); // Different color

        REQUIRE(hash1.getKey() != hash2.getKey());
        REQUIRE(hash1.getKey() != hash3.getKey());
        REQUIRE(hash2.getKey() != hash3.getKey());
    }

    SECTION("Zobrist equality operator works correctly") {
        Zobrist hash1, hash2;

        REQUIRE(hash1 == hash2); // Both empty

        hash1.updatePsqtHash(White, Pawn, A1);
        REQUIRE_FALSE(hash1 == hash2); // Now different

        hash2.updatePsqtHash(White, Pawn, A1);
        REQUIRE(hash1 == hash2); // Same again
    }
}

TEST_CASE("Zobrist initialization", "[zobrist][initialization]") {

    SECTION("Default constructor creates zero hash") {
        Zobrist hash;
        REQUIRE(hash.getKey() == 0);
    }

    SECTION("Fresh zobrist objects are equal") {
        Zobrist hash1, hash2;
        REQUIRE(hash1 == hash2);
        REQUIRE(hash1.getKey() == hash2.getKey());
    }
}
