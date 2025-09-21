#include "coordinator.hpp"
#include <bit>

namespace motor {

    void Coordinator::makeMove(const ChessMove& move) {
        // Analyze move effects before making the move
        MoveDelta delta = analyzeMove(move);

        // Push NNUE state
        network.pushState();

        // Apply NNUE changes
        applyMoveDelta(delta);

        // Make the move on the board
        board.makeMove(move);
    }

    void Coordinator::unmakeMove(const ChessMove& move) {
        // Unmake the move on the board first
        board.unmakeMove(move);

        // Pop NNUE state
        network.popState();
    }

    void Coordinator::makeNullMove() {
        network.pushState();
        board.makeNullMove();
    }

    void Coordinator::unmakeNullMove() {
        board.unmakeNullMove();
        network.popState();
    }

    MoveDelta Coordinator::analyzeMove(const ChessMove& move) const {
        MoveDelta delta;
        const Color us = board.getSideToMove();
        const Color them = (us == White) ? Black : White;
        const Square from = move.getFrom();
        const Square to = move.getTo();
        const Piece movingPiece = board.getPiece(from);
        const Piece capturedPiece = board.getPiece(to);

        switch (move.getMoveType()) {
            case NORMAL: {
                // Handle capture first (remove captured piece)
                if (capturedPiece != Null_Piece) {
                    delta.changes.push_back({MoveDelta::UNSET, them, capturedPiece, to});
                }

                // Remove piece from old square, add to new square
                delta.changes.push_back({MoveDelta::UNSET, us, movingPiece, from});
                delta.changes.push_back({MoveDelta::SET, us, movingPiece, to});
                break;
            }

            case PROMOTION: {
                // Handle capture
                if (capturedPiece != Null_Piece) {
                    delta.changes.push_back({MoveDelta::UNSET, them, capturedPiece, to});
                }

                // Remove pawn, add promoted piece
                delta.changes.push_back({MoveDelta::UNSET, us, Pawn, from});
                delta.changes.push_back({MoveDelta::SET, us, move.getPromotionPiece(), to});
                break;
            }

            case EN_PASSANT: {
                // Remove captured pawn
                const Square capturedPawnSquare = static_cast<Square>(to + (us == White ? -8 : 8));
                delta.changes.push_back({MoveDelta::UNSET, them, Pawn, capturedPawnSquare});

                // Move attacking pawn
                delta.changes.push_back({MoveDelta::UNSET, us, Pawn, from});
                delta.changes.push_back({MoveDelta::SET, us, Pawn, to});
                break;
            }

            case CASTLING: {
                // Move king
                delta.changes.push_back({MoveDelta::UNSET, us, King, from});
                delta.changes.push_back({MoveDelta::SET, us, King, to});

                // Move rook
                Square rookFrom, rookTo;
                if (to > from) { // Kingside
                    rookFrom = static_cast<Square>(us == White ? H1 : H8);
                    rookTo = static_cast<Square>(us == White ? F1 : F8);
                } else { // Queenside
                    rookFrom = static_cast<Square>(us == White ? A1 : A8);
                    rookTo = static_cast<Square>(us == White ? D1 : D8);
                }

                delta.changes.push_back({MoveDelta::UNSET, us, Rook, rookFrom});
                delta.changes.push_back({MoveDelta::SET, us, Rook, rookTo});
                break;
            }
        }

        return delta;
    }

    void Coordinator::applyMoveDelta(const MoveDelta& delta) {
        for (const auto& change : delta.changes) {
            switch (change.op) {
                case MoveDelta::SET:
                    network.updateAccumulator<MoveDelta::SET>(change.piece, change.color, change.square);
                    break;
                case MoveDelta::UNSET:
                    network.updateAccumulator<MoveDelta::UNSET>(change.piece, change.color, change.square);
                    break;
            }
        }
    }

} // namespace motor
