#include "catch2/catch_all.hpp"
#include "rays.hpp"
#include "attacks.hpp"
#include <bit>

using namespace motor::rays;
using namespace motor::attacks;

// Helper function to count bits in a bitboard
int popcount(std::uint64_t bb) {
    return std::popcount(bb);
}

// Helper function to get list of squares from bitboard
std::vector<int> bitboardToSquares(std::uint64_t bb) {
    std::vector<int> squares;
    for (int i = 0; i < 64; ++i) {
        if (bb & (1ULL << i)) {
            squares.push_back(i);
        }
    }
    return squares;
}

TEST_CASE("Ray Between Table - Basic Properties", "[rays]") {
    SECTION("Ray from square to itself is empty") {
        for (int square = 0; square < 64; ++square) {
            REQUIRE(RAY_BETWEEN[square][square] == 0);
        }
    }

    SECTION("Ray is symmetric in terms of existence") {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                // If there's a ray from->to, there should be a ray to->from
                // (though the contents will be different)
                bool has_ray_from_to = (RAY_BETWEEN[from][to] != 0);
                bool has_ray_to_from = (RAY_BETWEEN[to][from] != 0);
                
                // For aligned squares, both should exist
                int file_diff = std::abs((to % 8) - (from % 8));
                int rank_diff = std::abs((to / 8) - (from / 8));
                bool aligned = (file_diff == 0 || rank_diff == 0 || file_diff == rank_diff);
                
                if (aligned && from != to) {
                    REQUIRE(has_ray_from_to);
                    REQUIRE(has_ray_to_from);
                    REQUIRE(RAY_BETWEEN[from][to] != RAY_BETWEEN[to][from]);
                }
            }
        }
    }
}

TEST_CASE("Ray Between Table - Rook Moves", "[rays][rook]") {
    SECTION("Horizontal rays") {
        // E1 to H1: should include F1, G1, H1
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(7, 0)]; // e1 to h1
        std::vector<int> expected = {squareFrom(5, 0), squareFrom(6, 0), squareFrom(7, 0)}; // f1, g1, h1
        
        for (int sq : expected) {
            REQUIRE((ray & (1ULL << sq)) != 0);
        }
        REQUIRE(popcount(ray) == 3);
    }

    SECTION("Vertical rays") {
        // E1 to E4: should include E2, E3, E4
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(4, 3)]; // e1 to e4
        std::vector<int> expected = {squareFrom(4, 1), squareFrom(4, 2), squareFrom(4, 3)}; // e2, e3, e4
        
        for (int sq : expected) {
            REQUIRE((ray & (1ULL << sq)) != 0);
        }
        REQUIRE(popcount(ray) == 3);
    }

    SECTION("Vertical ray from E1 to E8") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(4, 7)]; // e1 to e8
        REQUIRE(popcount(ray) == 7); // e2, e3, e4, e5, e6, e7, e8
    }
}

TEST_CASE("Ray Between Table - Bishop Moves", "[rays][bishop]") {
    SECTION("Diagonal ray from E1 to H4") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(7, 3)]; // e1 to h4
        std::vector<int> expected = {squareFrom(5, 1), squareFrom(6, 2), squareFrom(7, 3)}; // f2, g3, h4
        
        for (int sq : expected) {
            REQUIRE((ray & (1ULL << sq)) != 0);
        }
        REQUIRE(popcount(ray) == 3);
    }

    SECTION("Anti-diagonal ray from E1 to B4") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(1, 3)]; // e1 to b4
        std::vector<int> expected = {squareFrom(3, 1), squareFrom(2, 2), squareFrom(1, 3)}; // d2, c3, b4
        
        for (int sq : expected) {
            REQUIRE((ray & (1ULL << sq)) != 0);
        }
        REQUIRE(popcount(ray) == 3);
    }
}

TEST_CASE("Ray Between Table - Knight Moves", "[rays][knight]") {
    SECTION("Knight move from E1 to F3") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(5, 2)]; // e1 to f3
        REQUIRE(popcount(ray) == 1);
        REQUIRE((ray & (1ULL << squareFrom(5, 2))) != 0); // f3
    }

    SECTION("Knight move from E1 to G2") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(6, 1)]; // e1 to g2
        REQUIRE(popcount(ray) == 1);
        REQUIRE((ray & (1ULL << squareFrom(6, 1))) != 0); // g2
    }

    SECTION("Knight move from D4 to E6") {
        std::uint64_t ray = RAY_BETWEEN[squareFrom(3, 3)][squareFrom(4, 5)]; // d4 to e6
        REQUIRE(popcount(ray) == 1);
        REQUIRE((ray & (1ULL << squareFrom(4, 5))) != 0); // e6
    }
}

TEST_CASE("Ray Between Table - Non-aligned Squares", "[rays][misc]") {
    SECTION("Non-aligned squares return empty ray") {
        // E1 to F3 is a knight move (should work)
        std::uint64_t knight_ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(5, 2)];
        REQUIRE(knight_ray != 0);
        
        // E1 to F4 is neither straight nor knight move (should be empty)
        std::uint64_t empty_ray = RAY_BETWEEN[squareFrom(4, 0)][squareFrom(5, 3)];
        REQUIRE(empty_ray == 0);
    }

    SECTION("Edge cases") {
        // Corner to corner same rank
        std::uint64_t ray = RAY_BETWEEN[squareFrom(0, 0)][squareFrom(7, 0)]; // a1 to h1
        REQUIRE(popcount(ray) == 7);
        
        // Corner to corner diagonal
        std::uint64_t diag_ray = RAY_BETWEEN[squareFrom(0, 0)][squareFrom(7, 7)]; // a1 to h8
        REQUIRE(popcount(diag_ray) == 7);
    }
}

TEST_CASE("Ray Between Table - Specific Examples", "[rays][examples]") {
    SECTION("E4 to E7 - vertical ray") {
        int e4 = squareFrom(4, 3);
        int e7 = squareFrom(4, 6);
        std::uint64_t ray = RAY_BETWEEN[e4][e7];
        
        // Should include e5, e6, e7
        REQUIRE((ray & (1ULL << squareFrom(4, 4))) != 0); // e5
        REQUIRE((ray & (1ULL << squareFrom(4, 5))) != 0); // e6
        REQUIRE((ray & (1ULL << squareFrom(4, 6))) != 0); // e7
        REQUIRE(popcount(ray) == 3);
    }

    SECTION("A1 to H8 - diagonal ray") {
        int a1 = squareFrom(0, 0);
        int h8 = squareFrom(7, 7);
        std::uint64_t ray = RAY_BETWEEN[a1][h8];
        
        // Should include b2, c3, d4, e5, f6, g7, h8
        std::vector<int> expected = {
            squareFrom(1, 1), squareFrom(2, 2), squareFrom(3, 3),
            squareFrom(4, 4), squareFrom(5, 5), squareFrom(6, 6), squareFrom(7, 7)
        };
        
        for (int sq : expected) {
            REQUIRE((ray & (1ULL << sq)) != 0);
        }
        REQUIRE(popcount(ray) == 7);
    }
}
