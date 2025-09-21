#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <random>
#include <mutex>
#include <chrono>
#include "coordinator.hpp"
#include "search.hpp"

namespace motor::datagen {

    enum class GameResult {
        WHITE_WIN = 1,
        DRAW = 0,
        BLACK_WIN = -1
    };

    struct TrainingPosition {
        std::string fen;
        std::int32_t score;  // From search
        GameResult result;   // Final game result
        
        std::string toString() const {
            return fen + " | " + std::to_string(score) + " | " + std::to_string(static_cast<int>(result));
        }
    };

    struct DataGenConfig {
        int num_games = 1000;
        int num_threads = 4;
        int search_depth = 4;
        int min_random_moves = 8;
        int max_random_moves = 9;
        int max_game_length = 400;  // Max moves per game to prevent infinite games
        std::string output_file = "training_data.txt";
        bool verbose = false;
        
        // Game termination conditions
        int draw_score_threshold = 10;    // Positions with |eval| < 10 can be draws
        int mate_score_threshold = 25000; // Mate found
        int fifty_move_rule = 100;        // 50-move rule
    };

    class DataGenerator {
    private:
        DataGenConfig config;
        std::atomic<int> games_completed{0};
        std::atomic<int> positions_generated{0};
        std::mutex file_mutex;
        std::ofstream output_file;
        
        // Random number generation
        thread_local static std::mt19937 rng;

    public:
        explicit DataGenerator(const DataGenConfig& cfg);
        ~DataGenerator();

        // Main interface
        void generateData();
        void stopGeneration();

        // Statistics
        int getGamesCompleted() const { return games_completed.load(); }
        int getPositionsGenerated() const { return positions_generated.load(); }

    private:
        // Game playing
        GameResult playGame(std::vector<TrainingPosition>& positions);
        void makeRandomMoves(Coordinator& engine, int num_moves);
        ChessMove selectRandomMove(Coordinator& engine);
        ChessMove selectBestMove(Coordinator& engine, int depth);
        
        // Game state evaluation
        bool isGameOver(const Coordinator& engine, GameResult& result);
        bool isInsufficientMaterial(const Coordinator& engine);
        bool isThreefoldRepetition(const std::vector<std::string>& position_history);
        
        // File I/O
        void writePositions(const std::vector<TrainingPosition>& positions);

        // Threading
        void workerThread();
        
        // Utility
        static GameResult scoreToResult(std::int32_t score);
        void logProgress();
        
        std::atomic<bool> should_stop{false};
    };

    // Utility functions
    std::string formatDuration(std::chrono::milliseconds duration);
    void printDataGenStats(const DataGenerator& generator);

} // namespace motor::datagen
