#include "perft.hpp"
#include <iomanip>
#include <iostream>
#include <vector>
#include "chess_move.hpp"

namespace motor::benchmarks {

    const std::vector<Perft::TestCase> Perft::TEST_CASES = {
            // Starting position
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20, "Starting position depth 1"},
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400, "Starting position depth 2"},
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902, "Starting position depth 3"},
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281, "Starting position depth 4"},
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609, "Starting position depth 5"},
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324, "Starting position depth 6"},

            // Position 2 (Kiwipete)
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48, "Kiwipete depth 1"},
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039, "Kiwipete depth 2"},
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862, "Kiwipete depth 3"},
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603, "Kiwipete depth 4"},

            // Position 3
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14, "Position 3 depth 1"},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191, "Position 3 depth 2"},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812, "Position 3 depth 3"},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238, "Position 3 depth 4"},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624, "Position 3 depth 5"},

            // Position 4 (En passant and castling)
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6, "Position 4 depth 1"},
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264, "Position 4 depth 2"},
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467, "Position 4 depth 3"},
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333, "Position 4 depth 4"},

            // Position 5
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 1, 44, "Position 5 depth 1"},
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 2, 1486, "Position 5 depth 2"},
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379, "Position 5 depth 3"},
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487, "Position 5 depth 4"},

            // Position 6
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 1, 46, "Position 6 depth 1"},
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 2, 2079, "Position 6 depth 2"},
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890,
             "Position 6 depth 3"},
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594,
             "Position 6 depth 4"}};

    std::uint64_t Perft::perft(motor::Board &board, int depth) {
        if (depth == 0) {
            return 1;
        }

        motor::MoveList moves;
        motor::MoveGenerator::generateAllMoves(board, moves);

        if (depth == 1) {
            return moves.size();
        }

        std::uint64_t nodes = 0;
        for (std::uint8_t i = 0; i < moves.size(); ++i) {
            const auto move = moves.getMove(i);
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    void Perft::perftDivide(motor::Board &board, int depth) {
        if (depth <= 0) {
            std::cout << "Invalid depth for perft divide\n";
            return;
        }

        motor::MoveList moves;
        motor::MoveGenerator::generateAllMoves(board, moves);

        std::uint64_t totalNodes = 0;
        std::cout << "\nPerft divide at depth " << depth << ":\n";
        std::cout << "Move      Nodes\n";
        std::cout << "----      -----\n";

        for (std::uint8_t i = 0; i < moves.size(); ++i) {
            const ChessMove move = moves.getMove(i);
            board.makeMove(move);

            const std::uint64_t nodeCount = (depth == 1) ? 1 : perft(board, depth - 1);
            totalNodes += nodeCount;

            std::cout << std::left << std::setw(10) << move.toString() << std::right << std::setw(10) << nodeCount
                      << "\n";

            board.unmakeMove(move);
        }

        std::cout << "\nTotal: " << totalNodes << " nodes\n";
        std::cout << "Moves: " << static_cast<int>(moves.size()) << "\n";
    }

    Perft::PerftResult Perft::timedPerft(motor::Board &board, int depth) {
        const auto startTime = std::chrono::high_resolution_clock::now();
        const std::uint64_t nodes = perft(board, depth);
        const auto endTime = std::chrono::high_resolution_clock::now();

        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        const double nodesPerSecond =
                (duration.count() > 0) ? static_cast<double>(nodes) / static_cast<double>(duration.count()) * 1000.0
                                       : 0.0;

        return {nodes, duration, nodesPerSecond};
    }

    void Perft::printResult(int depth, const PerftResult &result) {
        std::cout << "Depth " << std::setw(2) << depth << ": " << std::setw(12) << result.nodes << " nodes"
                  << " in " << std::setw(6) << result.duration.count() << "ms"
                  << " (" << std::setw(10) << std::fixed << std::setprecision(0) << result.nodesPerSecond << " N/s)"
                  << std::endl;
    }

    void Perft::benchmark(motor::Board &board, int maxDepth) {
        std::cout << "\n=== Perft Benchmark ===" << std::endl;
        std::cout << "FEN: " << board.toFen() << std::endl;
        std::cout << "========================" << std::endl;

        for (int depth = 1; depth <= maxDepth; ++depth) {
            const PerftResult result = timedPerft(board, depth);
            printResult(depth, result);

            // Stop if it's taking too long (> 30 seconds)
            if (result.duration.count() > 30000) {
                std::cout << "Stopping benchmark - depth " << depth << " took over 30 seconds" << std::endl;
                break;
            }
        }
        std::cout << "========================" << std::endl;
    }

    bool Perft::verify(motor::Board &board, int depth, std::uint64_t expected) {
        const std::uint64_t actual = perft(board, depth);
        return actual == expected;
    }

    void Perft::runTestSuite() {
        std::cout << "\n=== Perft Test Suite ===" << std::endl;
        std::cout << "Testing move generation correctness..." << std::endl;
        std::cout << "=====================================" << std::endl;

        int passed = 0;
        int failed = 0;

        for (const auto &testCase: TEST_CASES) {
            motor::Board board(testCase.fen);
            const bool success = verify(board, testCase.depth, testCase.expected);

            if (success) {
                std::cout << "✓ PASS: " << testCase.description << std::endl;
                passed++;
            } else {
                const std::uint64_t actual = perft(board, testCase.depth);
                std::cout << "✗ FAIL: " << testCase.description << " (Expected: " << testCase.expected
                          << ", Got: " << actual << ")" << std::endl;
                failed++;
            }
        }

        std::cout << "=====================================" << std::endl;
        std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;

        if (failed == 0) {
            std::cout << "🎉 All tests passed! Move generation is correct." << std::endl;
        } else {
            std::cout << "❌ Some tests failed. Check move generation implementation." << std::endl;
        }
    }

} // namespace motor::benchmarks
