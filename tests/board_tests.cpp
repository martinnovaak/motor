#include "catch2/catch_all.hpp"
#include "board.hpp"
#include "chess_move.hpp"
#include <vector>

using namespace motor;

// Helper functions for testing
namespace {
    // Check if a square is occupied by a specific piece
    bool hasPieceAt(const Board& board, Square square, Piece piece, Color color) {
        return board.getPiece(square) == piece &&
               (board.getPieces(color, piece) & (1ULL << square)) != 0;
    }

    // Count total pieces on board
    int countPieces(const Board& board) {
        int count = 0;
        for (int sq = A1; sq <= H8; ++sq) {
            if (board.getPiece(static_cast<Square>(sq)) != Null_Piece) {
                count++;
            }
        }
        return count;
    }

    // Count specific piece type
    int countPiece(const Board& board, Color color, Piece piece) {
        return std::popcount(board.getPieces(color, piece));
    }
}

TEST_CASE("Board initialization", "[board][initialization]") {
    SECTION("Default constructor creates starting position") {
        Board board;

        // Check starting piece positions
        REQUIRE(hasPieceAt(board, A1, Rook, White));
        REQUIRE(hasPieceAt(board, B1, Knight, White));
        REQUIRE(hasPieceAt(board, C1, Bishop, White));
        REQUIRE(hasPieceAt(board, D1, Queen, White));
        REQUIRE(hasPieceAt(board, E1, King, White));
        REQUIRE(hasPieceAt(board, F1, Bishop, White));
        REQUIRE(hasPieceAt(board, G1, Knight, White));
        REQUIRE(hasPieceAt(board, H1, Rook, White));

        // Check white pawns
        for (Square sq = A2; sq <= H2; sq = static_cast<Square>(sq + 1)) {
            REQUIRE(hasPieceAt(board, sq, Pawn, White));
        }

        // Check black pieces
        REQUIRE(hasPieceAt(board, A8, Rook, Black));
        REQUIRE(hasPieceAt(board, E8, King, Black));

        // Check black pawns
        for (Square sq = A7; sq <= H7; sq = static_cast<Square>(sq + 1)) {
            REQUIRE(hasPieceAt(board, sq, Pawn, Black));
        }

        // Check empty squares
        for (Square sq = A3; sq <= H6; sq = static_cast<Square>(sq + 1)) {
            REQUIRE(board.getPiece(sq) == Null_Piece);
        }

        // Check game state
        REQUIRE(board.getSideToMove() == White);
        REQUIRE(board.canCastle(WHITE_KINGSIDE));
        REQUIRE(board.canCastle(WHITE_QUEENSIDE));
        REQUIRE(board.canCastle(BLACK_KINGSIDE));
        REQUIRE(board.canCastle(BLACK_QUEENSIDE));
        REQUIRE(board.getEnpassantSquare() == Null_Square);

        // Check piece counts
        REQUIRE(countPieces(board) == 32);
        REQUIRE(countPiece(board, White, Pawn) == 8);
        REQUIRE(countPiece(board, Black, Pawn) == 8);
        REQUIRE(countPiece(board, White, King) == 1);
        REQUIRE(countPiece(board, Black, King) == 1);
    }

    SECTION("Custom FEN constructor") {
        const std::string testFen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
        Board board(testFen);

        REQUIRE(board.getSideToMove() == White);
        REQUIRE(hasPieceAt(board, E4, Pawn, White));
        REQUIRE(hasPieceAt(board, E5, Pawn, Black));
        REQUIRE(hasPieceAt(board, C6, Knight, Black));
        REQUIRE(hasPieceAt(board, F3, Knight, White));
        REQUIRE(board.getPiece(G1) == Null_Piece); // Knight moved from g1 to f3
    }
}

TEST_CASE("FEN parsing and generation", "[board][fen]") {
    SECTION("Parse starting position FEN") {
        const std::string startingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        Board board(startingFen);

        REQUIRE(board.getSideToMove() == White);
        REQUIRE(board.canCastle(ALL_CASTLING));
        REQUIRE(board.getEnpassantSquare() == Null_Square);
    }

    SECTION("Parse position with en passant") {
        const std::string fenWithEnPassant = "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3";
        Board board(fenWithEnPassant);

        REQUIRE(board.getEnpassantSquare() == F6);
        REQUIRE(hasPieceAt(board, E5, Pawn, White));
        REQUIRE(hasPieceAt(board, F5, Pawn, Black));
    }

    SECTION("Parse position with partial castling rights") {
        const std::string fenPartialCastling = "r3k2r/8/8/8/8/8/8/R3K2R w Kq - 0 1";
        Board board(fenPartialCastling);

        REQUIRE(board.canCastle(WHITE_KINGSIDE));
        REQUIRE_FALSE(board.canCastle(WHITE_QUEENSIDE));
        REQUIRE_FALSE(board.canCastle(BLACK_KINGSIDE));
        REQUIRE(board.canCastle(BLACK_QUEENSIDE));
    }

    SECTION("Parse position with no castling rights") {
        const std::string fenNoCastling = "r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1";
        Board board(fenNoCastling);

        REQUIRE_FALSE(board.canCastle(WHITE_KINGSIDE));
        REQUIRE_FALSE(board.canCastle(WHITE_QUEENSIDE));
        REQUIRE_FALSE(board.canCastle(BLACK_KINGSIDE));
        REQUIRE_FALSE(board.canCastle(BLACK_QUEENSIDE));
    }

    SECTION("Parse position with black to move") {
        const std::string fenBlackToMove = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        Board board(fenBlackToMove);

        REQUIRE(board.getSideToMove() == Black);
        REQUIRE(board.getEnpassantSquare() == E3);
        REQUIRE(hasPieceAt(board, E4, Pawn, White));
    }
}

TEST_CASE("Board state queries", "[board][state]") {
    SECTION("King square detection") {
        Board board;

        REQUIRE(board.getKingSquare(White) == E1);
        REQUIRE(board.getKingSquare(Black) == E8);
        REQUIRE(board.getKingSquare() == E1); // Default to side to move
    }

    SECTION("Occupancy bitboards") {
        Board board;

        const std::uint64_t whiteOccupancy = board.getSideOccupancy(White);
        const std::uint64_t blackOccupancy = board.getSideOccupancy(Black);
        const std::uint64_t totalOccupancy = board.getOccupancy();

        REQUIRE(std::popcount(whiteOccupancy) == 16);
        REQUIRE(std::popcount(blackOccupancy) == 16);
        REQUIRE(std::popcount(totalOccupancy) == 32);
        REQUIRE((whiteOccupancy & blackOccupancy) == 0); // No overlap
        REQUIRE((whiteOccupancy | blackOccupancy) == totalOccupancy);
    }

    SECTION("Piece bitboards") {
        Board board;

        // White pawns should be on rank 2
        const std::uint64_t whitePawns = board.getPieces(White, Pawn);
        REQUIRE(whitePawns == ranks[RANK_2]);

        // Black pawns should be on rank 7
        const std::uint64_t blackPawns = board.getPieces(Black, Pawn);
        REQUIRE(blackPawns == ranks[RANK_7]);

        // White rooks should be on a1 and h1
        const std::uint64_t whiteRooks = board.getPieces(White, Rook);
        REQUIRE(whiteRooks == ((1ULL << A1) | (1ULL << H1)));
    }
}

TEST_CASE("Hash key consistency", "[board][hashing]") {
    SECTION("Starting position has consistent hash") {
        Board board1;
        Board board2("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        REQUIRE(board1.getHashKey() == board2.getHashKey());
    }

    SECTION("Different positions have different hashes") {
        Board board1;
        Board board2("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

        REQUIRE(board1.getHashKey() != board2.getHashKey());
    }
}

TEST_CASE("FEN round-trip", "[board][fen]") {
    SECTION("Starting position FEN round-trip") {
        const std::string startingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        Board board(startingFen);

        std::string generatedFen = board.toFen();

        Board board2(generatedFen);
        REQUIRE(board.getHashKey() == board2.getHashKey());
    }

    SECTION("Complex position FEN round-trip") {
        const std::string complexFen = "r1bq1rk1/ppp2ppp/2n2n2/2bpp3/2B1P3/3P1N2/PPP2PPP/RNBQ1RK1 w - - 4 8";
        Board board(complexFen);

        std::string generatedFen = board.toFen();
        Board board2(generatedFen);
        REQUIRE(board.getHashKey() == board2.getHashKey());
    }
}

TEST_CASE("Complex positions", "[board][complex]") {
    SECTION("Sicilian Defense position") {
        const std::string sicilian = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2";
        Board board(sicilian);

        REQUIRE(board.getSideToMove() == White);
        REQUIRE(hasPieceAt(board, C5, Pawn, Black));
        REQUIRE(hasPieceAt(board, E4, Pawn, White));
        REQUIRE(board.getEnpassantSquare() == C6);
        REQUIRE(countPieces(board) == 32);
    }

    SECTION("Middle game position") {
        const std::string middlegame = "r1bq1rk1/ppp2ppp/2n2n2/2bpp3/2B1P3/3P1N2/PPP2PPP/RNBQ1RK1 w - - 4 8";
        Board board(middlegame);

        REQUIRE(board.getSideToMove() == White);
        REQUIRE_FALSE(board.canCastle(ALL_CASTLING)); // No castling rights
        REQUIRE(board.getEnpassantSquare() == Null_Square);

        // Check some specific pieces
        REQUIRE(hasPieceAt(board, G1, King, White));
        REQUIRE(hasPieceAt(board, G8, King, Black));
        REQUIRE(hasPieceAt(board, C4, Bishop, White));
        REQUIRE(hasPieceAt(board, C5, Bishop, Black));
    }

    SECTION("Endgame position") {
        const std::string endgame = "8/8/8/8/8/8/k1K5/8 w - - 0 1";
        Board board(endgame);

        REQUIRE(countPieces(board) == 2);
        REQUIRE(hasPieceAt(board, C2, King, White));
        REQUIRE(hasPieceAt(board, A2, King, Black));
        REQUIRE_FALSE(board.canCastle(ALL_CASTLING));
        REQUIRE(board.isPawnEndgame()); // Only kings
    }
}
