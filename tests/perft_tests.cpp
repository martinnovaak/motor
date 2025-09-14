#include "catch2/catch_all.hpp"
#include "perft.hpp"

using namespace motor;
using namespace motor::benchmarks;

TEST_CASE("Perft - Starting Position", "[perft]") {
    Board board;

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 20); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 400); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 8902); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 197281); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 4865609); }
}

TEST_CASE("Perft - Kiwipete Position", "[perft]") {
    Board board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 48); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 2039); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 97862); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 4085603); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 193690690); }

    SECTION("Depth 6") { REQUIRE(Perft::perft(board, 6) == 8031647685); }
}

TEST_CASE("Perft - Position 3", "[perft]") {
    Board board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 14); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 191); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 2812); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 43238); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 674624); }

    SECTION("Depth 6") { REQUIRE(Perft::perft(board, 6) == 11030083); }
}

TEST_CASE("Perft - Position 4 (Castling and En Passant)", "[perft]") {
    Board board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 6); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 264); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 9467); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 422333); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 15833292); }
}

TEST_CASE("Perft - Position 5", "[perft]") {
    Board board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 44); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 1486); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 62379); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 2103487); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 89941194); }
}

TEST_CASE("Perft - Position 6", "[perft]") {
    Board board("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 46); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 2079); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 89890); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 3894594); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 164075551); }

    SECTION("Depth 6") { REQUIRE(Perft::perft(board, 6) == 6923051137); }
}

TEST_CASE("Perft - Position Double Discovery", "[perft]") {
    Board board("6r1/2q2pp1/1PB2k2/3P1P2/5Q1B/8/6K1/7R b - - 0 56");

    SECTION("Depth 1") { REQUIRE(Perft::perft(board, 1) == 1); }

    SECTION("Depth 2") { REQUIRE(Perft::perft(board, 2) == 48); }

    SECTION("Depth 3") { REQUIRE(Perft::perft(board, 3) == 1060); }

    SECTION("Depth 4") { REQUIRE(Perft::perft(board, 4) == 42723); }

    SECTION("Depth 5") { REQUIRE(Perft::perft(board, 5) == 981168); }

    SECTION("Depth 6") { REQUIRE(Perft::perft(board, 6) == 37765954); }
}
