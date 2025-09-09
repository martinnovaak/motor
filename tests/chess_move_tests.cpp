#include "../src/core/chess_move.hpp"
#include "catch2/catch_all.hpp"

TEST_CASE("ChessMove basic functionality", "[move]") {
    SECTION("Normal move creation and access") {
        auto move = ChessMove::createNormalMove(E2, E4);
        REQUIRE(move.getFrom() == E2);
        REQUIRE(move.getTo() == E4);
        REQUIRE(move.getMoveType() == NORMAL);
        REQUIRE_FALSE(move.isNull());
    }

    SECTION("Promotion move") {
        auto move = ChessMove::createPromotionMove(A7, A8, Queen);
        REQUIRE(move.getMoveType() == PROMOTION);
        REQUIRE(move.getPromotionPiece() == Queen);
        REQUIRE(move.toString() == "a7a8q");
    }

    SECTION("UCI parsing") {
        auto move = ChessMoveUtils::parseUCIMove("e2e4");
        REQUIRE(move.getFrom() == E2);
        REQUIRE(move.getTo() == E4);

        auto promMove = ChessMoveUtils::parseUCIMove("a7a8q");
        REQUIRE(promMove.getMoveType() == PROMOTION);
        REQUIRE(promMove.getPromotionPiece() == Queen);
    }

    SECTION("Null move") {
        auto nullMove = ChessMove::nullMove();
        REQUIRE(nullMove.isNull());
        REQUIRE(nullMove.toString() == "0000");
    }
}
