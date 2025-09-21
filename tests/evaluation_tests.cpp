#include "catch2/catch_all.hpp"
#include "coordinator.hpp"
#include "move_generator.hpp"
#include <vector>
#include <string>

using namespace motor;

TEST_CASE("NNUE Evaluation Consistency - Same Position Different Paths", "[evaluation][consistency]") {
    
    SECTION("Make/Unmake sequence returns to original evaluation") {
        Coordinator engine;
        
        // Get initial evaluation
        int initial_eval = engine.evaluate();
        std::string initial_fen = engine.toFen();
        
        // Make some moves
        std::vector<ChessMove> moves = {
            ChessMove::createNormalMove(E2, E4),
            ChessMove::createNormalMove(E7, E5),
            ChessMove::createNormalMove(G1, F3),
            ChessMove::createNormalMove(B8, C6)
        };
        
        // Apply moves
        for (const auto& move : moves) {
            engine.makeMove(move);
        }
        
        // Unmake moves in reverse order
        for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
            engine.unmakeMove(*it);
        }
        
        // Should be back to original position
        int final_eval = engine.evaluate();
        std::string final_fen = engine.toFen();
        
        REQUIRE(initial_eval == final_eval);
        REQUIRE(initial_fen == final_fen);
    }
    
    SECTION("Different move orders to same position give same evaluation") {
        Coordinator engine1, engine2;
        
        // Path 1: e4, e5, Nf3, Nc6
        std::vector<ChessMove> path1 = {
            ChessMove::createNormalMove(E2, E4),
            ChessMove::createNormalMove(E7, E5),
            ChessMove::createNormalMove(G1, F3),
            ChessMove::createNormalMove(B8, C6)
        };
        
        // Path 2: Nf3, Nc6, e4, e5 (different order to same position)
        std::vector<ChessMove> path2 = {
            ChessMove::createNormalMove(G1, F3),
            ChessMove::createNormalMove(B8, C6),
            ChessMove::createNormalMove(E2, E4),
            ChessMove::createNormalMove(E7, E5)
        };
        
        // Apply path 1
        for (const auto& move : path1) {
            engine1.makeMove(move);
        }
        
        // Apply path 2
        for (const auto& move : path2) {
            engine2.makeMove(move);
        }
        
        // Both should have same evaluation and FEN
        int eval1 = engine1.evaluate();
        int eval2 = engine2.evaluate();
        std::string fen1 = engine1.toFen();
        std::string fen2 = engine2.toFen();
        
        REQUIRE(eval1 == eval2);
        REQUIRE(fen1 == fen2);
    }
    
    SECTION("Transposition table scenario - A-B-C vs A-C-B") {
        Coordinator engine1, engine2;
        
        // Both start with d4
        ChessMove d4 = ChessMove::createNormalMove(D2, D4);
        engine1.makeMove(d4);
        engine2.makeMove(d4);
        
        ChessMove d5 = ChessMove::createNormalMove(D7, D5);
        engine1.makeMove(d5);
        engine2.makeMove(d5);
        
        // Now different orders:
        // Engine1: Nf3, then c4
        ChessMove nf3 = ChessMove::createNormalMove(G1, F3);
        ChessMove c4 = ChessMove::createNormalMove(C2, C4);
        
        engine1.makeMove(nf3);
        engine1.makeMove(ChessMove::createNormalMove(G8, F6)); // Black responds
        engine1.makeMove(c4);
        
        // Engine2: c4, then Nf3  
        engine2.makeMove(c4);
        engine2.makeMove(ChessMove::createNormalMove(G8, F6)); // Black responds
        engine2.makeMove(nf3);
        
        // Should have same evaluation
        int eval1 = engine1.evaluate();
        int eval2 = engine2.evaluate();
        
        REQUIRE(eval1 == eval2);
        REQUIRE(engine1.toFen() == engine2.toFen());
    }
    
    SECTION("Complex sequence with captures and special moves") {
        Coordinator engine;
        
        // Set up a position with captures
        std::string complex_fen = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
        engine.setFromFen(complex_fen);
        
        int initial_eval = engine.evaluate();
        
        // Make a capture and undo it
        ChessMove capture = ChessMove::createNormalMove(C4, F7); // Bxf7+
        engine.makeMove(capture);
        
        int after_capture_eval = engine.evaluate();
        REQUIRE(initial_eval != after_capture_eval); // Should be different
        
        engine.unmakeMove(capture);
        int restored_eval = engine.evaluate();
        
        REQUIRE(initial_eval == restored_eval);
        REQUIRE(engine.toFen() == complex_fen);
    }
    
    SECTION("Evaluation consistency with promotions") {
        // Set up a position where we can test pawn promotion
        std::string promotion_fen = "8/P7/8/8/8/8/8/4K2k w - - 0 1";
        Coordinator engine;
        engine.setFromFen(promotion_fen);
        
        int initial_eval = engine.evaluate();
        
        // Promote to queen
        ChessMove promote_queen = ChessMove::createPromotionMove(A7, A8, Queen);
        engine.makeMove(promote_queen);
        
        int after_promotion_eval = engine.evaluate();
        REQUIRE(initial_eval != after_promotion_eval);
        
        engine.unmakeMove(promote_queen);
        int restored_eval = engine.evaluate();
        
        REQUIRE(initial_eval == restored_eval);
        REQUIRE(engine.toFen() == promotion_fen);
    }
    
    SECTION("Evaluation consistency with en passant") {
        // Set up en passant position
        std::string en_passant_fen = "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3";
        Coordinator engine;
        engine.setFromFen(en_passant_fen);
        
        int initial_eval = engine.evaluate();
        
        // Make en passant capture
        ChessMove en_passant = ChessMove::createEnPassantMove(E5, F6);
        engine.makeMove(en_passant);
        
        int after_ep_eval = engine.evaluate();
        REQUIRE(initial_eval != after_ep_eval);
        
        engine.unmakeMove(en_passant);
        int restored_eval = engine.evaluate();
        
        REQUIRE(initial_eval == restored_eval);
        REQUIRE(engine.toFen() == en_passant_fen);
    }
    
    SECTION("Evaluation consistency with castling") {
        // Position where castling is possible
        std::string castling_fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
        Coordinator engine;
        engine.setFromFen(castling_fen);
        
        int initial_eval = engine.evaluate();
        
        // Castle kingside
        ChessMove castle = ChessMove::createCastlingMove(E1, G1);
        engine.makeMove(castle);
        
        int after_castle_eval = engine.evaluate();
        
        engine.unmakeMove(castle);
        int restored_eval = engine.evaluate();
        
        REQUIRE(initial_eval == restored_eval);
        REQUIRE(engine.toFen() == castling_fen);
    }
    
    SECTION("Deep move sequence consistency") {
        Coordinator engine;
        
        int initial_eval = engine.evaluate();
        
        // Make a long sequence of moves
        std::vector<ChessMove> long_sequence = {
            ChessMove::createNormalMove(E2, E4),
            ChessMove::createNormalMove(E7, E5),
            ChessMove::createNormalMove(G1, F3),
            ChessMove::createNormalMove(B8, C6),
            ChessMove::createNormalMove(F1, C4),
            ChessMove::createNormalMove(F8, C5),
            ChessMove::createNormalMove(D2, D3),
            ChessMove::createNormalMove(D7, D6),
            ChessMove::createNormalMove(B1, C3),
            ChessMove::createNormalMove(C8, E6)
        };
        
        // Apply all moves
        for (const auto& move : long_sequence) {
            engine.makeMove(move);
        }
        
        // Undo all moves
        for (auto it = long_sequence.rbegin(); it != long_sequence.rend(); ++it) {
            engine.unmakeMove(*it);
        }
        
        int final_eval = engine.evaluate();
        
        REQUIRE(initial_eval == final_eval);
        REQUIRE(engine.toFen() == std::string(Board::STARTING_FEN));
    }
}

TEST_CASE("NNUE Evaluation Deterministic", "[evaluation][deterministic]") {
    
    SECTION("Same position gives same evaluation multiple times") {
        std::string test_fen = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
        
        // Create multiple engines with same position
        Coordinator engine1(test_fen);
        Coordinator engine2(test_fen);
        Coordinator engine3;
        engine3.setFromFen(test_fen);
        
        int eval1 = engine1.evaluate();
        int eval2 = engine2.evaluate();
        int eval3 = engine3.evaluate();
        
        REQUIRE(eval1 == eval2);
        REQUIRE(eval2 == eval3);
        REQUIRE(eval1 == eval3);
    }
    
    SECTION("Evaluation remains consistent after null moves") {
        Coordinator engine;
        
        int initial_eval = engine.evaluate();
        
        // Make and undo null move
        engine.makeNullMove();
        engine.unmakeNullMove();
        
        int after_null_eval = engine.evaluate();
        
        REQUIRE(initial_eval == after_null_eval);
    }
}
