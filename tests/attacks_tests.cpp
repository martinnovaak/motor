#include "catch2/catch_all.hpp"
#include "attacks.hpp"
#include <vector>

using namespace motor::attacks;

// A helper function to create a bitboard from a list of squares
std::uint64_t bitboardFromSquares(std::vector<int> squares) {
  std::uint64_t bitboard = 0;
  for (int square : squares)
    bitboard |= squareToBitboard(square);
  return bitboard;
}

// =========================================================================
// Piece Attack Tests
// =========================================================================

TEST_CASE("Pawn Attacks", "[pawn]") {
  SECTION("White pawn on a2 attacks b3") {
    std::uint64_t pawns = squareToBitboard(squareFrom(0, 1)); // a2
    std::uint64_t expected_attacks = bitboardFromSquares({squareFrom(1, 2)}); // b3
    REQUIRE(pawnAttacks(White, pawns) == expected_attacks);
  }

  SECTION("White pawn on h2 attacks g3") {
    std::uint64_t pawns = squareToBitboard(squareFrom(7, 1)); // h2
    std::uint64_t expected_attacks = bitboardFromSquares({squareFrom(6, 2)}); // g3
    REQUIRE(pawnAttacks(White, pawns) == expected_attacks);
  }

  SECTION("Black pawn on a7 attacks b6") {
    std::uint64_t pawns = squareToBitboard(squareFrom(0, 6)); // a7
    std::uint64_t expected_attacks = bitboardFromSquares({squareFrom(1, 5)}); // b6
    REQUIRE(pawnAttacks(Black, pawns) == expected_attacks);
  }
}

TEST_CASE("Knight Attacks Table", "[knight][precomputed]") {
  SECTION("Knight on a1 attacks b3 and c2") {
    std::uint64_t expected_attacks = bitboardFromSquares({
        squareFrom(1, 2), // b3
        squareFrom(2, 1)  // c2
    });
    REQUIRE(knightAttacks(squareFrom(0, 0)) == expected_attacks);
  }

  SECTION("Knight on e4 attacks 8 squares") {
    std::uint64_t expected_attacks = bitboardFromSquares({
        squareFrom(3, 1), // d2
        squareFrom(5, 1), // f2
        squareFrom(2, 2), // c3
        squareFrom(6, 2), // g3
        squareFrom(2, 4), // c5
        squareFrom(6, 4), // g5
        squareFrom(3, 5), // d6
        squareFrom(5, 5)  // f6
    });
    REQUIRE(knightAttacks(squareFrom(4, 3)) == expected_attacks);
  }
}

TEST_CASE("King Attacks Table", "[king][precomputed]") {
  SECTION("King on a1 attacks 3 squares") {
    std::uint64_t expected_attacks = bitboardFromSquares({
        squareFrom(0, 1), // a2
        squareFrom(1, 0), // b1
        squareFrom(1, 1)  // b2
    });
    REQUIRE(kingAttacks(squareFrom(0, 0)) == expected_attacks);
  }

  SECTION("King on e4 attacks 8 squares") {
    std::uint64_t expected_attacks = bitboardFromSquares({
        squareFrom(3, 2), // d3
        squareFrom(4, 2), // e3
        squareFrom(5, 2), // f3
        squareFrom(3, 3), // d4
        squareFrom(5, 3), // f4
        squareFrom(3, 4), // d5
        squareFrom(4, 4), // e5
        squareFrom(5, 4)  // f5
    });
    REQUIRE(kingAttacks(squareFrom(4, 3)) == expected_attacks);
  }
}

// =========================================================================
// Slider Attack Tests
// =========================================================================

TEST_CASE("Rook Attacks", "[slider][rook]") {
  SECTION("Rook on a1 on an empty board") {
    std::uint64_t empty_board = 0ULL;
    std::uint64_t a1_attacks = rookAttacks(0, empty_board); // a1 = 0

    std::uint64_t expected_attacks = 0ULL;
    for (int i = 1; i < 8; ++i)
      expected_attacks |= squareToBitboard(squareFrom(i, 0));
    for (int i = 1; i < 8; ++i)
      expected_attacks |= squareToBitboard(squareFrom(0, i));

    REQUIRE(a1_attacks == expected_attacks);
  }
}

TEST_CASE("Bishop Attacks", "[slider][bishop]") {
  SECTION("Bishop on c1 on an empty board") {
    std::uint64_t empty_board = 0ULL;
    std::uint64_t c1_attacks = bishopAttacks(2, empty_board); // c1 = 2

    std::uint64_t expected_attacks = 0ULL;
    // North-west ray from c1 to a3
    expected_attacks |= bitboardFromSquares({squareFrom(1, 1), squareFrom(0, 2)});
    // North-east ray from c1 to h6
    expected_attacks |= bitboardFromSquares({squareFrom(3, 1), squareFrom(4, 2),
                                             squareFrom(5, 3), squareFrom(6, 4),
                                             squareFrom(7, 5)});

    REQUIRE(c1_attacks == expected_attacks);
  }
}

TEST_CASE("Queen Attacks", "[slider][queen]") {
  SECTION("Queen on d4 on an empty board") {
    std::uint64_t empty_board = 0ULL;
    std::uint64_t d4_attacks = queenAttacks(27, empty_board); // d4 = 27

    // Expected attacks should be a combination of rook and bishop attacks on d4
    std::uint64_t expected_rook_attacks = 0ULL;
    for (int i = 0; i < 8; ++i)
      expected_rook_attacks |= squareToBitboard(squareFrom(i, 3)); // rank 4
    for (int i = 0; i < 8; ++i)
      expected_rook_attacks |= squareToBitboard(squareFrom(3, i)); // file d
    expected_rook_attacks ^= squareToBitboard(27); // remove center

    std::uint64_t expected_bishop_attacks = 0ULL;
    // a1-h8 diagonal
    expected_bishop_attacks |= bitboardFromSquares({squareFrom(0, 0),
                                                    squareFrom(1, 1),
                                                    squareFrom(2, 2),
                                                    squareFrom(4, 4),
                                                    squareFrom(5, 5),
                                                    squareFrom(6, 6),
                                                    squareFrom(7, 7)});
    // a7-g1 diagonal
    expected_bishop_attacks |=
        bitboardFromSquares({squareFrom(0, 6), squareFrom(1, 5),
                             squareFrom(2, 4), squareFrom(4, 2),
                             squareFrom(5, 1), squareFrom(6, 0)});

    REQUIRE(d4_attacks == (expected_rook_attacks | expected_bishop_attacks));
  }
}
