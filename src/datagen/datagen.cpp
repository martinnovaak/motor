#include "datagen.hpp"
#include <iostream>
#include <algorithm>
#include <unordered_set>

namespace motor::datagen {

    // Thread-local random number generator
    thread_local std::mt19937 DataGenerator::rng(std::chrono::steady_clock::now().time_since_epoch().count());

    DataGenerator::DataGenerator(const DataGenConfig& cfg) : config(cfg) {
        output_file.open(config.output_file);
        if (!output_file.is_open()) {
            throw std::runtime_error("Failed to open output file: " + config.output_file);
        }

        if (config.verbose) {
            std::cout << "Data generation started:\n"
                      << "  Games: " << config.num_games << "\n"
                      << "  Threads: " << config.num_threads << "\n"
                      << "  Search depth: " << config.search_depth << "\n"
                      << "  Random moves: " << config.min_random_moves << "-" << config.max_random_moves << "\n"
                      << "  Output: " << config.output_file << "\n" << std::endl;
        }
    }

    DataGenerator::~DataGenerator() {
        if (output_file.is_open()) {
            output_file.close();
        }
    }

    void DataGenerator::generateData() {
        std::vector<std::thread> threads;
        
        // Launch worker threads
        for (int i = 0; i < config.num_threads; ++i) {
            threads.emplace_back(&DataGenerator::workerThread, this);
        }
        
        // Progress monitoring thread
        std::thread progress_thread([this]() {
            while (!should_stop && games_completed < config.num_games) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                if (config.verbose) {
                    logProgress();
                }
            }
        });
        
        // Wait for all workers to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        should_stop = true;
        if (progress_thread.joinable()) {
            progress_thread.join();
        }
        
        if (config.verbose) {
            std::cout << "\nData generation completed!\n"
                      << "Total games: " << games_completed << "\n"
                      << "Total positions: " << positions_generated << "\n" << std::endl;
        }
    }

    void DataGenerator::stopGeneration() {
        should_stop = true;
    }

    void DataGenerator::workerThread() {
        while (!should_stop && games_completed < config.num_games) {
            std::vector<TrainingPosition> positions;
            
            try {
                GameResult result = playGame(positions);
                
                if (!positions.empty()) {
                    // Set the game result for all positions
                    for (auto& pos : positions) {
                        pos.result = result;
                    }
                    
                    writePositions(positions);
                    positions_generated += positions.size();
                }
                
                games_completed++;
                
            } catch (const std::exception& e) {
                if (config.verbose) {
                    std::cerr << "Error in game: " << e.what() << std::endl;
                }
            }
        }
    }

    GameResult DataGenerator::playGame(std::vector<TrainingPosition>& positions) {
        Coordinator engine;
        Search searcher(&engine);
        
        std::vector<std::string> position_history;
        int move_count = 0;
        int fifty_move_counter = 0;
        
        // Make random opening moves
        std::uniform_int_distribution<int> random_moves_dist(config.min_random_moves, config.max_random_moves);
        int num_random_moves = random_moves_dist(rng);
        makeRandomMoves(engine, num_random_moves);
        
        // Play the game
        while (move_count < config.max_game_length && !should_stop) {
            // Check for game termination
            GameResult result;
            if (isGameOver(engine, result)) {
                return result;
            }
            
            // Check fifty-move rule
            if (fifty_move_counter >= config.fifty_move_rule) {
                return GameResult::DRAW;
            }
            
            // Check threefold repetition
            std::string current_fen = engine.toFen();
            position_history.push_back(current_fen);
            if (isThreefoldRepetition(position_history)) {
                return GameResult::DRAW;
            }
            
            // Search for best move
            SearchLimits limits;
            limits.depth = config.search_depth;
            
            SearchResult search_result = searcher.search(limits);
            
            if (search_result.best_move.isNull()) {
                // No legal moves - checkmate or stalemate already handled by isGameOver
                break;
            }
            
            // Record position for training (skip early random moves)
            if (move_count >= num_random_moves) {
                TrainingPosition training_pos;
                training_pos.fen = current_fen;
                training_pos.score = search_result.score;
                // Result will be set after game completion
                positions.push_back(training_pos);
            }
            
            // Check for mate scores
            if (std::abs(search_result.score) > config.mate_score_threshold) {
                return (search_result.score > 0) ? 
                    (engine.getSideToMove() == White ? GameResult::WHITE_WIN : GameResult::BLACK_WIN) :
                    (engine.getSideToMove() == White ? GameResult::BLACK_WIN : GameResult::WHITE_WIN);
            }

            // Make the move
            engine.makeMove(search_result.best_move);
            move_count++;

            fifty_move_counter = engine.getBoard().getFiftyMoveCounter();
        }
        
        // Game ended without clear result - likely draw
        return GameResult::DRAW;
    }

    void DataGenerator::makeRandomMoves(Coordinator& engine, int num_moves) {
        for (int i = 0; i < num_moves && !should_stop; ++i) {
            ChessMove move = selectRandomMove(engine);
            if (move.isNull()) {
                break;  // No legal moves
            }
            engine.makeMove(move);
        }
    }

    ChessMove DataGenerator::selectRandomMove(Coordinator& engine) {
        MoveList moves;
        engine.generateAllMoves(moves);
        
        if (moves.empty()) {
            return ChessMove::nullMove();
        }
        
        std::uniform_int_distribution<int> move_dist(0, moves.size() - 1);
        return moves.getMove(move_dist(rng));
    }

    bool DataGenerator::isGameOver(const Coordinator& engine, GameResult& result) {
        MoveList moves;
        MoveGenerator::generateAllMoves(engine.getBoard(), const_cast<MoveList&>(moves));
        
        if (moves.empty()) {
            if (engine.isInCheck()) {
                // Checkmate
                result = (engine.getSideToMove() == White) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
                return true;
            } else {
                // Stalemate
                result = GameResult::DRAW;
                return true;
            }
        }
        
        // Check for insufficient material
        if (isInsufficientMaterial(engine)) {
            result = GameResult::DRAW;
            return true;
        }
        
        return false;
    }

    bool DataGenerator::isInsufficientMaterial(const Coordinator& engine) {
        const Board& board = engine.getBoard();
        
        // Count material
        int white_pieces = 0, black_pieces = 0;
        bool white_has_major = false, black_has_major = false;
        
        for (int color = White; color <= Black; ++color) {
            for (int piece = Pawn; piece < King; ++piece) {
                int count = std::popcount(board.getPieces(static_cast<Color>(color), static_cast<Piece>(piece)));
                
                if (color == White) {
                    white_pieces += count;
                    if (piece == Rook || piece == Queen) white_has_major = true;
                } else {
                    black_pieces += count;
                    if (piece == Rook || piece == Queen) black_has_major = true;
                }
            }
        }
        
        // Basic insufficient material detection
        if (white_pieces <= 1 && black_pieces <= 1 && !white_has_major && !black_has_major) {
            return true; // K vs K, K+N vs K, K+B vs K, etc.
        }
        
        return false;
    }

    bool DataGenerator::isThreefoldRepetition(const std::vector<std::string>& position_history) {
        if (position_history.size() < 6) return false;  // Need at least 3 repetitions
        
        const std::string& current_pos = position_history.back();
        int count = 0;
        
        // Check last few positions for repetition
        for (int i = static_cast<int>(position_history.size()) - 1; i >= 0; i -= 2) {  // Same side to move
            if (position_history[i] == current_pos) {
                count++;
                if (count >= 3) return true;
            }
            if (position_history.size() - i > 8) break;  // Don't check too far back
        }
        
        return false;
    }

    void DataGenerator::writePositions(const std::vector<TrainingPosition>& positions) {
        std::lock_guard<std::mutex> lock(file_mutex);
        
        for (const auto& pos : positions) {
            output_file << pos.toString() << "\n";
        }
        output_file.flush();
    }

    GameResult DataGenerator::scoreToResult(std::int32_t score) {
        const int mate_threshold = 25000;  // Use constant instead of config
        if (score > mate_threshold) return GameResult::WHITE_WIN;
        if (score < -mate_threshold) return GameResult::BLACK_WIN;
        return GameResult::DRAW;
    }

    void DataGenerator::logProgress() {
        int completed = games_completed.load();
        int positions = positions_generated.load();
        double progress = static_cast<double>(completed) / config.num_games * 100.0;

        std::cout << "Progress: " << completed << "/" << config.num_games
                  << " games (" << std::fixed << std::setprecision(1) << progress << "%) "
                  << "Positions: " << positions << std::endl;
    }

    std::string formatDuration(std::chrono::milliseconds duration) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration - hours - minutes);

        return std::to_string(hours.count()) + "h " +
               std::to_string(minutes.count()) + "m " +
               std::to_string(seconds.count()) + "s";
    }

    void printDataGenStats(const DataGenerator& generator) {
        std::cout << "=== Data Generation Statistics ===\n"
                  << "Games completed: " << generator.getGamesCompleted() << "\n"
                  << "Positions generated: " << generator.getPositionsGenerated() << "\n"
                  << "===================================" << std::endl;
    }

} // namespace motor::datagen
