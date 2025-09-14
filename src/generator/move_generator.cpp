#include "move_generator.hpp"
#include <iomanip>

namespace motor {

    void MoveGenerator::generateAllMoves(const Board &board, MoveList &moves) {
        moves.clear();
        const Color side = board.getSideToMove();
        const bool inCheck = board.isInCheck();

        generatePawnMoves(board, side, inCheck, false, moves);
        generateKnightMoves(board, side, inCheck, false, moves);
        generateBishopMoves(board, side, inCheck, false, moves);
        generateRookMoves(board, side, inCheck, false, moves);
        generateQueenMoves(board, side, inCheck, false, moves);
        generateKingMoves(board, side, false, moves);

        // Castling is only legal when not in check
        if (!inCheck) {
            generateCastlingMoves(board, side, moves);
        }
    }

    void MoveGenerator::generateCaptures(const Board &board, MoveList &moves) {
        moves.clear();
        const Color side = board.getSideToMove();
        const bool inCheck = board.isInCheck();

        generatePawnMoves(board, side, inCheck, true, moves);
        generateKnightMoves(board, side, inCheck, true, moves);
        generateBishopMoves(board, side, inCheck, true, moves);
        generateRookMoves(board, side, inCheck, true, moves);
        generateQueenMoves(board, side, inCheck, true, moves);
        generateKingMoves(board, side, true, moves);
    }

    void MoveGenerator::generateQuietMoves(const Board &board, MoveList &moves) {
        moves.clear();
        const Color side = board.getSideToMove();
        const bool inCheck = board.isInCheck();

        // Generate quiet pawn moves (pushes, not captures)
        generatePawnPushes(board, side, inCheck, moves);
        generateKnightMoves(board, side, inCheck, false, moves);
        generateBishopMoves(board, side, inCheck, false, moves);
        generateRookMoves(board, side, inCheck, false, moves);
        generateQueenMoves(board, side, inCheck, false, moves);
        generateKingMoves(board, side, false, moves);

        if (!inCheck) {
            generateCastlingMoves(board, side, moves);
        }
    }

    void MoveGenerator::generatePawnMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                          MoveList &moves) {
        if (!onlyCaptures) {
            generatePawnPushes(board, side, inCheck, moves);
        }
        generatePawnCaptures(board, side, inCheck, moves);
        generatePromotions(board, side, inCheck, onlyCaptures, moves);
        generateEnPassant(board, side, inCheck, moves);
    }

    void MoveGenerator::generatePawnPushes(const Board &board, Color side, bool inCheck, MoveList &moves) {
        const std::uint64_t pawns = board.getPieces(side, Pawn);
        const std::uint64_t empty = ~board.getOccupancy();
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        // Pawns not on promotion ranks and not diagonally pinned
        const std::uint64_t rank7 = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
        std::uint64_t pushablePawns = pawns & ~rank7 & ~pinDiagonal;

        // Single pushes
        std::uint64_t singlePushes;
        if (side == White) {
            singlePushes = shiftNorth(pushablePawns & ~pinOrthogonal) |
                           (shiftNorth(pushablePawns & pinOrthogonal) & pinOrthogonal);
        } else {
            singlePushes = shiftSouth(pushablePawns & ~pinOrthogonal) |
                           (shiftSouth(pushablePawns & pinOrthogonal) & pinOrthogonal);
        }
        singlePushes &= empty;

        // Calculate double pushes before consuming singlePushes
        const std::uint64_t rank3 = (side == White) ? ranks[RANK_3] : ranks[RANK_6];
        std::uint64_t doublePushes;
        if (side == White) {
            doublePushes = shiftNorth(singlePushes & rank3) & empty & checkMask;
        } else {
            doublePushes = shiftSouth(singlePushes & rank3) & empty & checkMask;
        }

        singlePushes &= checkMask;

        // Process single pushes
        while (singlePushes) {
            const Square to = popLSB(singlePushes);
            const Square from = static_cast<Square>(to + (side == White ? -8 : 8));
            moves.push_back(ChessMove::createNormalMove(from, to));
        }

        while (doublePushes) {
            const Square to = popLSB(doublePushes);
            const Square from = static_cast<Square>(to + (side == White ? -16 : 16));
            moves.push_back(ChessMove::createNormalMove(from, to));
        }
    }

    void MoveGenerator::generatePawnCaptures(const Board &board, Color side, bool inCheck, MoveList &moves) {
        const Color opponent = (side == White) ? Black : White;
        const std::uint64_t pawns = board.getPieces(side, Pawn);
        const std::uint64_t enemies = board.getSideOccupancy(opponent);
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        // Pawns not on promotion ranks and not orthogonally pinned
        const std::uint64_t rank7 = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
        const std::uint64_t capturingPawns = pawns & ~rank7 & ~pinOrthogonal;

        std::uint64_t leftCaptures, rightCaptures;

        if (side == White) {
            leftCaptures = shiftNorthWest(capturingPawns & ~pinDiagonal) |
                           (shiftNorthWest(capturingPawns & pinDiagonal) & pinDiagonal);
            rightCaptures = shiftNorthEast(capturingPawns & ~pinDiagonal) |
                            (shiftNorthEast(capturingPawns & pinDiagonal) & pinDiagonal);
        } else {
            leftCaptures = shiftSouthWest(capturingPawns & ~pinDiagonal) |
                           (shiftSouthWest(capturingPawns & pinDiagonal) & pinDiagonal);
            rightCaptures = shiftSouthEast(capturingPawns & ~pinDiagonal) |
                            (shiftSouthEast(capturingPawns & pinDiagonal) & pinDiagonal);
        }

        leftCaptures &= enemies & checkMask;
        rightCaptures &= enemies & checkMask;

        while (leftCaptures) {
            const Square to = popLSB(leftCaptures);
            const Square from = static_cast<Square>(to + (side == White ? -7 : 9));
            moves.push_back(ChessMove::createNormalMove(from, to));
        }

        while (rightCaptures) {
            const Square to = popLSB(rightCaptures);
            const Square from = static_cast<Square>(to + (side == White ? -9 : 7));
            moves.push_back(ChessMove::createNormalMove(from, to));
        }
    }

    void MoveGenerator::generatePromotions(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                           MoveList &moves) {
        const Color opponent = (side == White) ? Black : White;
        const std::uint64_t pawns = board.getPieces(side, Pawn);
        const std::uint64_t enemies = board.getSideOccupancy(opponent);
        const std::uint64_t empty = ~board.getOccupancy();
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        const std::uint64_t rank7 = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
        const std::uint64_t promotingPawns = pawns & rank7;

        if (!promotingPawns)
            return;

        // Promotion captures
        const std::uint64_t capturingPawns = promotingPawns & ~pinOrthogonal;
        std::uint64_t leftPromotions, rightPromotions;

        if (side == White) {
            leftPromotions = shiftNorthWest(capturingPawns & ~pinDiagonal) |
                             (shiftNorthWest(capturingPawns & pinDiagonal) & pinDiagonal);
            rightPromotions = shiftNorthEast(capturingPawns & ~pinDiagonal) |
                              (shiftNorthEast(capturingPawns & pinDiagonal) & pinDiagonal);
        } else {
            leftPromotions = shiftSouthWest(capturingPawns & ~pinDiagonal) |
                             (shiftSouthWest(capturingPawns & pinDiagonal) & pinDiagonal);
            rightPromotions = shiftSouthEast(capturingPawns & ~pinDiagonal) |
                              (shiftSouthEast(capturingPawns & pinDiagonal) & pinDiagonal);
        }

        leftPromotions &= enemies & checkMask;
        rightPromotions &= enemies & checkMask;

        while (leftPromotions) {
            const Square to = popLSB(leftPromotions);
            const Square from = static_cast<Square>(to + (side == White ? -7 : 9));
            moves.push_back(ChessMove::createPromotionMove(from, to, Queen));
            if (!onlyCaptures) {
                moves.push_back(ChessMove::createPromotionMove(from, to, Knight));
                moves.push_back(ChessMove::createPromotionMove(from, to, Rook));
                moves.push_back(ChessMove::createPromotionMove(from, to, Bishop));
            }
        }

        while (rightPromotions) {
            const Square to = popLSB(rightPromotions);
            const Square from = static_cast<Square>(to + (side == White ? -9 : 7));
            moves.push_back(ChessMove::createPromotionMove(from, to, Queen));
            if (!onlyCaptures) {
                moves.push_back(ChessMove::createPromotionMove(from, to, Knight));
                moves.push_back(ChessMove::createPromotionMove(from, to, Rook));
                moves.push_back(ChessMove::createPromotionMove(from, to, Bishop));
            }
        }

        // Quiet promotions (non-captures)
        if (!onlyCaptures) {
            const std::uint64_t pushingPawns = promotingPawns & ~pinDiagonal;
            std::uint64_t quietPromotions;

            if (side == White) {
                quietPromotions = shiftNorth(pushingPawns & ~pinOrthogonal) |
                                  (shiftNorth(pushingPawns & pinOrthogonal) & pinOrthogonal);
            } else {
                quietPromotions = shiftSouth(pushingPawns & ~pinOrthogonal) |
                                  (shiftSouth(pushingPawns & pinOrthogonal) & pinOrthogonal);
            }

            quietPromotions &= empty & checkMask;

            while (quietPromotions) {
                const Square to = popLSB(quietPromotions);
                const Square from = static_cast<Square>(to + (side == White ? -8 : 8));
                moves.push_back(ChessMove::createPromotionMove(from, to, Queen));
                moves.push_back(ChessMove::createPromotionMove(from, to, Knight));
                moves.push_back(ChessMove::createPromotionMove(from, to, Rook));
                moves.push_back(ChessMove::createPromotionMove(from, to, Bishop));
            }
        }
    }

    void MoveGenerator::generateEnPassant(const Board &board, Color side, bool inCheck, MoveList &moves) {
        const Square epSquare = board.getEnpassantSquare();
        if (epSquare == Null_Square)
            return;

        const Color opponent = (side == White) ? Black : White;
        const std::uint64_t pawns = board.getPieces(side, Pawn);
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();
        const std::uint64_t pinDiagonal = board.getPinDiagonal();

        // Square of the pawn to be captured
        const Square capturedPawnSquare = static_cast<Square>(epSquare + (side == White ? -8 : 8));
        const std::uint64_t capturedPawnBB = 1ULL << capturedPawnSquare;

        // Pawns that can potentially capture en passant
        const std::uint64_t epCandidates = attacks::pawnAttacks(opponent, 1ULL << epSquare) & pawns & ~pinOrthogonal;

        std::uint64_t validEPPawns = epCandidates;

        // Filter out diagonally pinned pawns that can't move to the ep square
        if (pinDiagonal) {
            const std::uint64_t epSquareBB = 1ULL << epSquare;
            validEPPawns &= ~pinDiagonal | (pinDiagonal & ((epSquareBB & pinDiagonal) ? full_board : 0));
        }

        // Check if en passant would leave the king in check from a horizontal attack
        const Square kingSquare = board.getKingSquare(side);
        const std::uint64_t kingBB = 1ULL << kingSquare;
        const std::uint64_t enemyRooksQueens = board.getPieces(opponent, Rook) | board.getPieces(opponent, Queen);

        while (validEPPawns) {
            const Square from = popLSB(validEPPawns);
            const std::uint64_t fromBB = 1ULL << from;

            // Check if the en passant move would expose the king to horizontal attack
            const std::uint64_t occupancyAfterEP =
                    (board.getOccupancy() ^ fromBB ^ capturedPawnBB) | (1ULL << epSquare);
            const std::uint64_t kingAttacks = attacks::rookAttacks(kingSquare, occupancyAfterEP);

            if (!(kingAttacks & enemyRooksQueens)) {
                // Check if move satisfies check mask in case we're in check
                if (!inCheck || (board.getCheckers() & capturedPawnBB) || ((1ULL << epSquare) & checkMask)) {
                    moves.push_back(ChessMove::createEnPassantMove(from, epSquare));
                }
            }
        }
    }

    void MoveGenerator::generateKnightMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                            MoveList &moves) {
        const std::uint64_t knights =
                board.getPieces(side, Knight) & ~(board.getPinDiagonal() | board.getPinOrthogonal());
        const std::uint64_t friendlyPieces = board.getSideOccupancy(side);
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;

        std::uint64_t knightBB = knights;
        while (knightBB) {
            const Square from = popLSB(knightBB);
            std::uint64_t targets = attacks::knightAttacks(from) & ~friendlyPieces & checkMask;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }
    }

    void MoveGenerator::generateBishopMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                            MoveList &moves) {
        const std::uint64_t bishops = board.getPieces(side, Bishop);
        const std::uint64_t friendlyPieces = board.getSideOccupancy(side);
        const std::uint64_t occupancy = board.getOccupancy();
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        // Non-pinned bishops
        std::uint64_t nonPinnedBishops = bishops & ~pinDiagonal & ~pinOrthogonal;
        while (nonPinnedBishops) {
            const Square from = popLSB(nonPinnedBishops);
            std::uint64_t targets = attacks::bishopAttacks(from, occupancy) & ~friendlyPieces & checkMask;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }

        // Diagonally pinned bishops
        std::uint64_t pinnedBishops = bishops & pinDiagonal & ~pinOrthogonal;
        while (pinnedBishops) {
            const Square from = popLSB(pinnedBishops);
            std::uint64_t targets = attacks::bishopAttacks(from, occupancy) & ~friendlyPieces & checkMask & pinDiagonal;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }
    }

    void MoveGenerator::generateRookMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                          MoveList &moves) {
        const std::uint64_t rooks = board.getPieces(side, Rook);
        const std::uint64_t friendlyPieces = board.getSideOccupancy(side);
        const std::uint64_t occupancy = board.getOccupancy();
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        // Non-pinned rooks
        std::uint64_t nonPinnedRooks = rooks & ~pinDiagonal & ~pinOrthogonal;
        while (nonPinnedRooks) {
            const Square from = popLSB(nonPinnedRooks);
            std::uint64_t targets = attacks::rookAttacks(from, occupancy) & ~friendlyPieces & checkMask;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }

        // Orthogonally pinned rooks
        std::uint64_t pinnedRooks = rooks & pinOrthogonal & ~pinDiagonal;
        while (pinnedRooks) {
            const Square from = popLSB(pinnedRooks);
            std::uint64_t targets = attacks::rookAttacks(from, occupancy) & ~friendlyPieces & checkMask & pinOrthogonal;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }
    }

    void MoveGenerator::generateQueenMoves(const Board &board, Color side, bool inCheck, bool onlyCaptures,
                                           MoveList &moves) {
        const std::uint64_t queens = board.getPieces(side, Queen);
        const std::uint64_t friendlyPieces = board.getSideOccupancy(side);
        const std::uint64_t occupancy = board.getOccupancy();
        const std::uint64_t checkMask = inCheck ? board.getCheckMask() : full_board;
        const std::uint64_t pinDiagonal = board.getPinDiagonal();
        const std::uint64_t pinOrthogonal = board.getPinOrthogonal();

        // Non-pinned queens
        std::uint64_t nonPinnedQueens = queens & ~pinDiagonal & ~pinOrthogonal;
        while (nonPinnedQueens) {
            const Square from = popLSB(nonPinnedQueens);
            std::uint64_t targets = attacks::queenAttacks(from, occupancy) & ~friendlyPieces & checkMask;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }

        // Diagonally pinned queens (can only move diagonally)
        std::uint64_t diagonalPinnedQueens = queens & pinDiagonal & ~pinOrthogonal;
        while (diagonalPinnedQueens) {
            const Square from = popLSB(diagonalPinnedQueens);
            std::uint64_t targets = attacks::bishopAttacks(from, occupancy) & ~friendlyPieces & checkMask & pinDiagonal;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }

        // Orthogonally pinned queens (can only move orthogonally)
        std::uint64_t orthogonalPinnedQueens = queens & pinOrthogonal & ~pinDiagonal;
        while (orthogonalPinnedQueens) {
            const Square from = popLSB(orthogonalPinnedQueens);
            std::uint64_t targets = attacks::rookAttacks(from, occupancy) & ~friendlyPieces & checkMask & pinOrthogonal;

            if (onlyCaptures) {
                const Color opponent = (side == White) ? Black : White;
                targets &= board.getSideOccupancy(opponent);
            }

            while (targets) {
                const Square to = popLSB(targets);
                moves.push_back(ChessMove::createNormalMove(from, to));
            }
        }
    }

    void MoveGenerator::generateKingMoves(const Board &board, Color side, bool onlyCaptures, MoveList &moves) {
        const Square kingSquare = board.getKingSquare(side);
        const std::uint64_t friendlyPieces = board.getSideOccupancy(side);
        const std::uint64_t threats = board.getThreats();

        std::uint64_t targets = attacks::kingAttacks(kingSquare) & ~friendlyPieces & ~threats;

        if (onlyCaptures) {
            const Color opponent = (side == White) ? Black : White;
            targets &= board.getSideOccupancy(opponent);
        }

        while (targets) {
            const Square to = popLSB(targets);
            moves.push_back(ChessMove::createNormalMove(kingSquare, to));
        }
    }

    void MoveGenerator::generateCastlingMoves(const Board &board, Color side, MoveList &moves) {
        if (board.isInCheck())
            return; // Can't castle when in check

        const std::uint64_t occupancy = board.getOccupancy();
        const std::uint64_t threats = board.getThreats();

        if (side == White) {
            // White kingside castling
            if (board.canCastle(WHITE_KINGSIDE)) {
                if (!(occupancy & WHITE_KINGSIDE_PATH) && !(threats & WHITE_KINGSIDE_KING_PATH)) {
                    moves.push_back(ChessMove::createCastlingMove(E1, G1));
                }
            }

            // White queenside castling
            if (board.canCastle(WHITE_QUEENSIDE)) {
                if (!(occupancy & WHITE_QUEENSIDE_PATH) && !(threats & WHITE_QUEENSIDE_KING_PATH)) {
                    moves.push_back(ChessMove::createCastlingMove(E1, C1));
                }
            }
        } else {
            // Black kingside castling
            if (board.canCastle(BLACK_KINGSIDE)) {
                if (!(occupancy & BLACK_KINGSIDE_PATH) && !(threats & BLACK_KINGSIDE_KING_PATH)) {
                    moves.push_back(ChessMove::createCastlingMove(E8, G8));
                }
            }

            // Black queenside castling
            if (board.canCastle(BLACK_QUEENSIDE)) {
                if (!(occupancy & BLACK_QUEENSIDE_PATH) && !(threats & BLACK_QUEENSIDE_KING_PATH)) {
                    moves.push_back(ChessMove::createCastlingMove(E8, C8));
                }
            }
        }
    }

} // namespace motor
