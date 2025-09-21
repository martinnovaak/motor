#include "search.hpp"
#include <iostream>
#include <algorithm>

namespace motor {

    SearchResult Search::search(const SearchLimits& limits) {
        stop_flag = false;
        nodes_searched = 0;
        start_time = std::chrono::steady_clock::now();
        
        SearchResult result;
        int allocated_time = calculateTimeForMove(limits);
        
        MoveList root_moves;
        position->generateAllMoves(root_moves);
        
        if (root_moves.empty()) {
            return result;  // No legal moves
        }
        
        // If only one legal move, return immediately
        if (root_moves.size() == 1) {
            result.best_move = root_moves.getMove(0);
            result.score = 0;
            result.depth = 1;
            result.nodes = 1;
            result.time_ms = 1;
            return result;
        }
        
        orderMoves(root_moves);
        
        int best_score = -MATE_SCORE;
        ChessMove best_move = root_moves.getMove(0);
        
        // Iterative deepening
        int search_depth = limits.depth > 0 ? limits.depth : MAX_DEPTH;
        
        for (int depth = 1; depth <= search_depth; ++depth) {
            if (shouldStop(limits) || (allocated_time > 0 && isTimeUp(allocated_time))) {
                break;
            }
            
            int current_best_score = -MATE_SCORE;
            ChessMove current_best_move = best_move;
            
            // Search root moves
            for (std::uint8_t i = 0; i < root_moves.size(); ++i) {
                if (shouldStop(limits)) break;
                
                ChessMove move = root_moves.getMove(i);
                position->makeMove(move);
                
                int score = -alphaBeta(depth - 1, -MATE_SCORE, MATE_SCORE, 1);
                
                position->unmakeMove(move);
                
                if (score > current_best_score) {
                    current_best_score = score;
                    current_best_move = move;
                }
                
                // Check time periodically
                if (nodes_searched % 1000 == 0 && allocated_time > 0 && isTimeUp(allocated_time)) {
                    break;
                }
            }
            
            // Update best move if this depth completed
            if (!shouldStop(limits) && (!allocated_time || !isTimeUp(allocated_time))) {
                best_score = current_best_score;
                best_move = current_best_move;
                max_depth = depth;
                
                auto current_time = std::chrono::steady_clock::now();
                int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - start_time).count();
                
                sendInfo(depth, best_score, nodes_searched, elapsed_ms, best_move);
            }
            
            // Stop if we found a mate
            if (isMateScore(best_score)) {
                break;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        int total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        result.best_move = best_move;
        result.score = best_score;
        result.depth = max_depth;
        result.nodes = nodes_searched;
        result.time_ms = total_time;
        
        return result;
    }

    int Search::alphaBeta(int depth, int alpha, int beta, int ply) {
        nodes_searched++;
        
        if (depth <= 0) {
            return position->evaluate();
        }
        
        if (position->isDraw(ply)) {
            return 0;
        }
        
        MoveList moves;
        position->generateAllMoves(moves);
        
        if (moves.empty()) {
            // No legal moves - checkmate or stalemate
            if (position->isInCheck()) {
                return -MATE_SCORE + ply;  // Checkmate
            } else {
                return 0;  // Stalemate
            }
        }
        
        orderMoves(moves);
        
        int best_score = -MATE_SCORE;
        
        for (std::uint8_t i = 0; i < moves.size(); ++i) {
            if (shouldStop(SearchLimits{})) {
                break;
            }
            
            ChessMove move = moves.getMove(i);
            position->makeMove(move);
            
            int score = -alphaBeta(depth - 1, -beta, -alpha, ply + 1);
            
            position->unmakeMove(move);
            
            if (score > best_score) {
                best_score = score;
            }
            
            if (score > alpha) {
                alpha = score;
            }
            
            if (alpha >= beta) {
                break;  // Beta cutoff
            }
        }
        
        return best_score;
    }

    void Search::orderMoves(MoveList& moves) {
        // Simple move ordering: captures first, then other moves
        for (std::uint8_t i = 0; i < moves.size(); ++i) {
            moves[i] = getMoveScore(moves.getMove(i));
        }
    }

    int Search::getMoveScore(const ChessMove& move) {
        int score = 0;
        
        const Board& board = position->getBoard();
        const Piece capturedPiece = board.getPiece(move.getTo());
        const Piece movingPiece = board.getPiece(move.getFrom());
        
        // Capture scoring: captured_piece_value - moving_piece_value
        if (capturedPiece != Null_Piece) {
            static constexpr int piece_values[] = {100, 320, 330, 500, 900, 0}; // P, N, B, R, Q, K
            score += piece_values[capturedPiece] - piece_values[movingPiece] + 1000;
        }
        
        // Promotion bonus
        if (move.getMoveType() == PROMOTION) {
            static constexpr int piece_values[] = {100, 320, 330, 500, 900, 0};
            score += piece_values[move.getPromotionPiece()] + 800;
        }
        
        return score;
    }

    bool Search::shouldStop(const SearchLimits& limits) {
        if (stop_flag) return true;
        
        if (limits.nodes > 0 && nodes_searched >= limits.nodes) {
            return true;
        }
        
        if (limits.infinite) return false;
        
        return false;
    }

    int Search::calculateTimeForMove(const SearchLimits& limits) {
        if (limits.movetime > 0) {
            return limits.movetime;
        }
        
        if (limits.infinite) {
            return 0;  // No time limit
        }
        
        Color side = position->getSideToMove();
        int time_remaining = (side == White) ? limits.wtime : limits.btime;
        int increment = (side == White) ? limits.winc : limits.binc;
        
        if (time_remaining <= 0) return 0;
        
        // Simple time management: use 1/30 of remaining time + increment
        int allocated_time = time_remaining / 30 + increment;
        
        // Ensure we don't use too much time
        allocated_time = std::min(allocated_time, time_remaining / 2);
        
        return std::max(allocated_time, 10);  // At least 10ms
    }

    bool Search::isTimeUp(int allocated_time) {
        if (allocated_time <= 0) return false;
        
        auto current_time = std::chrono::steady_clock::now();
        int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time).count();
        
        return elapsed_ms >= allocated_time;
    }

    bool Search::isMateScore(int score) {
        return std::abs(score) > MATE_BOUND;
    }

    int Search::adjustMateScore(int score, int ply) {
        if (score > MATE_BOUND) {
            return score - ply;
        } else if (score < -MATE_BOUND) {
            return score + ply;
        }
        return score;
    }

    void Search::sendInfo(int depth, int score, int nodes, int time_ms, const ChessMove& best_move) {
        std::cout << "info depth " << depth 
                  << " score cp " << score
                  << " nodes " << nodes
                  << " time " << time_ms;
                  
        if (time_ms > 0) {
            std::cout << " nps " << (nodes * 1000 / time_ms);
        }
        
        std::cout << " pv " << best_move.toString()
                  << std::endl;
    }

} // namespace motor
