#pragma once

#include <atomic>
#include <chrono>
#include <limits>
#include "coordinator.hpp"
#include "move_generator.hpp"

namespace motor {

    struct SearchLimits {
        int depth = 0;               // Maximum depth (0 = no limit)
        int nodes = 0;               // Maximum nodes (0 = no limit)
        int movetime = 0;            // Exact time in ms (0 = no limit)
        int wtime = 0;               // White time remaining in ms
        int btime = 0;               // Black time remaining in ms
        int winc = 0;                // White increment per move in ms
        int binc = 0;                // Black increment per move in ms
        int movestogo = 0;           // Moves to next time control (0 = no limit)
        bool infinite = false;       // Search indefinitely
        bool ponder = false;         // Ponder mode
    };

    struct SearchResult {
        ChessMove best_move = ChessMove::nullMove();
        ChessMove ponder_move = ChessMove::nullMove();
        int score = 0;
        int depth = 0;
        int nodes = 0;
        int time_ms = 0;
    };

    class Search {
    private:
        Coordinator* position;
        std::atomic<bool> stop_flag{false};
        std::chrono::steady_clock::time_point start_time;
        
        // Search statistics
        int nodes_searched = 0;
        int max_depth = 0;
        
        // Constants
        static constexpr int MATE_SCORE = 30000;
        static constexpr int MATE_BOUND = 29000;
        static constexpr int MAX_DEPTH = 64;

    public:
        Search(Coordinator* pos) : position(pos) {}

        // Main search interface
        SearchResult search(const SearchLimits& limits);
        
        // Stop the search
        void stop() { stop_flag = true; }
        
        // Check if we should stop searching
        bool shouldStop(const SearchLimits& limits);
        
    private:
        // Core search algorithms
        int alphaBeta(int depth, int alpha, int beta, int ply);

        // Move ordering
        void orderMoves(MoveList& moves);
        int getMoveScore(const ChessMove& move);
        
        // Time management
        int calculateTimeForMove(const SearchLimits& limits);
        bool isTimeUp(int allocated_time);
        
        // Utility functions
        bool isMateScore(int score);
        int adjustMateScore(int score, int ply);
        void sendInfo(int depth, int score, int nodes, int time_ms, const ChessMove& best_move);
    };

} // namespace motor
