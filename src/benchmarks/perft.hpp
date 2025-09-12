#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include "board.hpp"
#include "move_generator.hpp"

namespace motor::benchmarks {

    class Perft {
    public:
        // Core perft functionality
        static std::uint64_t perft(motor::Board &board, int depth);
        static void perftDivide(motor::Board &board, int depth);

        // Benchmarking functionality
        static void benchmark(motor::Board &board, int maxDepth);
        static void runTestSuite();

        // Verification against known values
        static bool verify(motor::Board &board, int depth, std::uint64_t expected);

    private:
        struct PerftResult {
            std::uint64_t nodes;
            std::chrono::milliseconds duration;
            double nodesPerSecond;
        };

        static PerftResult timedPerft(motor::Board &board, int depth);
        static void printResult(int depth, const PerftResult &result);

        // Known perft values for starting position
        struct TestCase {
            std::string fen;
            int depth;
            std::uint64_t expected;
            std::string description;
        };

        static const std::vector<TestCase> TEST_CASES;
    };

} // namespace motor::benchmarks
