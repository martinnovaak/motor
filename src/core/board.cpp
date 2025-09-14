#include "board.hpp"
#include <cctype>
#include <sstream>
#include "rays.hpp"

namespace motor {

    void Board::reset() {
        PieceArray.fill(Null_Piece);
        Bitboards = {};
        SideOccupancy = {};
        AllOccupancy = 0;
        SideToMove = White;
        StateHistory.clear();
        StateHistory.reserve(1000);
        StateHistory.emplace_back();
        CurrentStateIndex = 0;
    }

    void Board::updateOccupancy() {
        SideOccupancy[White] = 0;
        SideOccupancy[Black] = 0;

        for (int piece = Pawn; piece < Null_Piece; ++piece) {
            SideOccupancy[White] |= Bitboards[White][piece];
            SideOccupancy[Black] |= Bitboards[Black][piece];
        }

        AllOccupancy = SideOccupancy[White] | SideOccupancy[Black];
    }

    void Board::updateZobristKeys(Color color, Piece piece, Square square) {
        getCurrentStateRef().Keys.updatePiece(color, piece, square);
    }

    void Board::setPiece(Color color, Piece piece, Square square) {
        const std::uint64_t mask = 1ULL << square;
        PieceArray[square] = piece;
        AllOccupancy |= mask;
        SideOccupancy[color] |= mask;
        Bitboards[color][piece] |= mask;
        updateZobristKeys(color, piece, square);
    }

    void Board::removePiece(Color color, Piece piece, Square square) {
        const std::uint64_t mask = ~(1ULL << square);
        PieceArray[square] = Null_Piece;
        AllOccupancy &= mask;
        SideOccupancy[color] &= mask;
        Bitboards[color][piece] &= mask;
        updateZobristKeys(color, piece, square);
    }

    void Board::movePiece(Color color, Piece piece, Square from, Square to) {
        const std::uint64_t fromTo = (1ULL << from) | (1ULL << to);
        PieceArray[to] = piece;
        PieceArray[from] = Null_Piece;
        AllOccupancy ^= fromTo;
        SideOccupancy[color] ^= fromTo;
        Bitboards[color][piece] ^= fromTo;

        // Update zobrist for both squares
        updateZobristKeys(color, piece, from);
        updateZobristKeys(color, piece, to);
    }

    void Board::updateCastlingRights(Square square) {
        auto &currentState = getCurrentStateRef();
        const std::uint8_t oldRights = currentState.CastlingRights;
        currentState.CastlingRights &= CASTLING_MASK[square];
        currentState.Keys.updateCastlingRights(oldRights, currentState.CastlingRights);
    }

    void Board::calculateThreats(Color us) {
        const Color them = (us == White) ? Black : White;
        std::uint64_t threats = 0;

        // Pawn attacks
        const std::uint64_t pawns = Bitboards[them][Pawn];
        threats |= motor::attacks::pawnAttacks(them, pawns);

        // Knight attacks
        std::uint64_t knights = Bitboards[them][Knight];
        while (knights) {
            const Square sq = static_cast<Square>(std::countr_zero(knights));
            threats |= motor::attacks::knightAttacks(sq);
            knights &= knights - 1; // Clear LSB
        }

        // King attacks (excluding our king for x-ray calculation)
        const std::uint64_t occupancy = AllOccupancy ^ Bitboards[us][King];

        // Bishop and queen diagonal attacks
        std::uint64_t bishops = Bitboards[them][Bishop] | Bitboards[them][Queen];
        while (bishops) {
            const Square sq = static_cast<Square>(std::countr_zero(bishops));
            threats |= motor::attacks::bishopAttacks(sq, occupancy);
            bishops &= bishops - 1;
        }

        // Rook and queen orthogonal attacks
        std::uint64_t rooks = Bitboards[them][Rook] | Bitboards[them][Queen];
        while (rooks) {
            const Square sq = static_cast<Square>(std::countr_zero(rooks));
            threats |= motor::attacks::rookAttacks(sq, occupancy);
            rooks &= rooks - 1;
        }

        // King attacks
        const Square enemyKing = getKingSquare(them);
        threats |= motor::attacks::kingAttacks(enemyKing);

        getCurrentStateRef().Threats = threats;
    }

    void Board::calculateCheckers(Color us) {
        const Square kingSquare = getKingSquare(us);
        getCurrentStateRef().Checkers = getAttackers(us, kingSquare);
    }

    void Board::calculatePins(Color us) {
        const Color them = (us == White) ? Black : White;
        const Square kingSquare = getKingSquare(us);

        std::uint64_t diagonalPins = 0;
        std::uint64_t orthogonalPins = 0;

        // Find diagonal pinners (bishops and queens)
        std::uint64_t pinners = motor::attacks::bishopAttacks(kingSquare, SideOccupancy[them]) &
                                (Bitboards[them][Bishop] | Bitboards[them][Queen]);

        while (pinners) {
            const Square pinnerSq = static_cast<Square>(std::countr_zero(pinners));
            // Use ray between king and pinner
            const std::uint64_t rayBetween = motor::rays::RAY_BETWEEN[kingSquare][pinnerSq];

            // Check if exactly one of our pieces is on this ray
            if (std::popcount(rayBetween & SideOccupancy[us]) == 1) {
                diagonalPins |= rayBetween;
            }
            pinners &= pinners - 1;
        }

        // Find orthogonal pinners (rooks and queens)
        pinners = motor::attacks::rookAttacks(kingSquare, SideOccupancy[them]) &
                  (Bitboards[them][Rook] | Bitboards[them][Queen]);

        while (pinners) {
            const Square pinnerSq = static_cast<Square>(std::countr_zero(pinners));
            // Use ray between king and pinner
            const std::uint64_t rayBetween = motor::rays::RAY_BETWEEN[kingSquare][pinnerSq];

            // Check if exactly one of our pieces is on this ray
            if (std::popcount(rayBetween & SideOccupancy[us]) == 1) {
                orthogonalPins |= rayBetween;
            }
            pinners &= pinners - 1;
        }

        auto &currentState = getCurrentStateRef();
        currentState.PinDiagonal = diagonalPins;
        currentState.PinOrthogonal = orthogonalPins;
    }

    std::uint64_t Board::getAttackers(Color us, Square square) const {
        const Color them = (us == White) ? Black : White;

        std::uint64_t attackers = 0;

        // Rook and queen attacks
        const std::uint64_t orthogonal = motor::attacks::rookAttacks(square, AllOccupancy);
        attackers |= orthogonal & (Bitboards[them][Rook] | Bitboards[them][Queen]);

        // Bishop and queen attacks
        const std::uint64_t diagonal = motor::attacks::bishopAttacks(square, AllOccupancy);
        attackers |= diagonal & (Bitboards[them][Bishop] | Bitboards[them][Queen]);

        // Knight attacks
        attackers |= motor::attacks::knightAttacks(square) & Bitboards[them][Knight];

        // King attacks
        attackers |= motor::attacks::kingAttacks(square) & Bitboards[them][King];

        // Pawn attacks
        const std::uint64_t squareBB = 1ULL << square;
        const std::uint64_t pawnAttacks = motor::attacks::pawnAttacks(us, squareBB);
        attackers |= pawnAttacks & Bitboards[them][Pawn];

        return attackers;
    }

    std::uint64_t Board::getAttackers(Color us, Square square, std::uint64_t occupancy) const {
        const Color them = (us == White) ? Black : White;

        std::uint64_t attackers = 0;

        // Rook and queen attacks
        const std::uint64_t orthogonal = motor::attacks::rookAttacks(square, occupancy);
        attackers |= orthogonal & (Bitboards[them][Rook] | Bitboards[them][Queen]);

        // Bishop and queen attacks
        const std::uint64_t diagonal = motor::attacks::bishopAttacks(square, occupancy);
        attackers |= diagonal & (Bitboards[them][Bishop] | Bitboards[them][Queen]);

        // Knight attacks
        attackers |= motor::attacks::knightAttacks(square) & Bitboards[them][Knight];

        // King attacks
        attackers |= motor::attacks::kingAttacks(square) & Bitboards[them][King];

        // Pawn attacks
        const std::uint64_t squareBB = 1ULL << square;
        const std::uint64_t pawnAttacks = motor::attacks::pawnAttacks(us, squareBB);
        attackers |= pawnAttacks & Bitboards[them][Pawn];

        return attackers;
    }

    void Board::updateBoardState() {
        calculateThreats(SideToMove);
        calculateCheckers(SideToMove);
        calculatePins(SideToMove);

        auto &currentState = getCurrentStateRef();

        // Set check mask
        currentState.CheckMask = full_board;
        if (currentState.Checkers) {
            const int checkerCount = std::popcount(currentState.Checkers);
            if (checkerCount == 1) {
                const Square checkerSquare = static_cast<Square>(std::countr_zero(currentState.Checkers));
                const Square kingSquare = getKingSquare(SideToMove);

                // Use ray between king and checker for check mask
                currentState.CheckMask = motor::rays::RAY_BETWEEN[kingSquare][checkerSquare];
            } else {
                // Double check - only king moves are legal
                currentState.CheckMask = 0;
            }
        }
    }

    std::pair<Color, Piece> Board::parseCharToPiece(char c) const {
        const bool isWhite = std::isupper(c);
        const Color color = isWhite ? White : Black;

        switch (std::tolower(c)) {
            case 'p':
                return {color, Pawn};
            case 'n':
                return {color, Knight};
            case 'b':
                return {color, Bishop};
            case 'r':
                return {color, Rook};
            case 'q':
                return {color, Queen};
            case 'k':
                return {color, King};
            default:
                return {White, Null_Piece};
        }
    }

    std::uint8_t Board::parseCastlingRights(const std::string &castling) const {
        std::uint8_t rights = 0;
        for (char c: castling) {
            switch (c) {
                case 'K':
                    rights |= WHITE_KINGSIDE;
                    break;
                case 'Q':
                    rights |= WHITE_QUEENSIDE;
                    break;
                case 'k':
                    rights |= BLACK_KINGSIDE;
                    break;
                case 'q':
                    rights |= BLACK_QUEENSIDE;
                    break;
            }
        }
        return rights;
    }

    Square Board::parseEnpassantSquare(const std::string &enpassant) const {
        if (enpassant == "-" || enpassant.length() < 2) {
            return Null_Square;
        }

        const char file = enpassant[0];
        const char rank = enpassant[1];

        if (file >= 'a' && file <= 'h' && rank >= '1' && rank <= '8') {
            return static_cast<Square>((file - 'a') + (rank - '1') * 8);
        }

        return Null_Square;
    }

    void Board::setFromFen(const std::string &fen) {
        reset();

        std::istringstream ss(fen);
        std::string boardStr, sideStr, castlingStr, enpassantStr, fiftyMoveStr;

        ss >> boardStr >> sideStr >> castlingStr >> enpassantStr >> fiftyMoveStr;

        auto &currentState = getCurrentStateRef();

        // Start batch update for efficiency
        currentState.Keys.batchUpdateStart();

        // Parse board position
        Square square = A8;
        for (char c: boardStr) {
            if (std::isdigit(c)) {
                square = static_cast<Square>(square + (c - '0'));
            } else if (c == '/') {
                square = static_cast<Square>(square - 16);
            } else {
                const auto [color, piece] = parseCharToPiece(c);
                if (piece != Null_Piece) {
                    const std::uint64_t mask = 1ULL << square;
                    PieceArray[square] = piece;
                    Bitboards[color][piece] |= mask;
                    updateZobristKeys(color, piece, square);
                }
                square = static_cast<Square>(square + 1);
            }
        }

        updateOccupancy();

        // Parse side to move
        SideToMove = (sideStr == "w") ? White : Black;
        if (SideToMove == Black) {
            currentState.Keys.updateSideToMove();
        }

        // Parse castling rights
        currentState.CastlingRights = parseCastlingRights(castlingStr);
        currentState.Keys.updateCastlingRights(0, currentState.CastlingRights);

        // Parse en passant square
        currentState.EnpassantSquare = parseEnpassantSquare(enpassantStr);
        currentState.Keys.updateEnPassant(Null_Square, currentState.EnpassantSquare);

        // Parse fifty-move counter
        if (!fiftyMoveStr.empty()) {
            currentState.FiftyMoveCounter = static_cast<std::uint8_t>(std::stoi(fiftyMoveStr));
        }

        // Update board state
        updateBoardState();
    }

    std::string Board::toFen() const {
        std::ostringstream fen;
        const auto &currentState = getCurrentState();

        // Board position
        for (int rank = 7; rank >= 0; --rank) {
            int emptyCount = 0;
            for (int file = 0; file < 8; ++file) {
                const Square square = static_cast<Square>(rank * 8 + file);
                const Piece piece = getPiece(square);

                if (piece == Null_Piece) {
                    emptyCount++;
                } else {
                    if (emptyCount > 0) {
                        fen << emptyCount;
                        emptyCount = 0;
                    }

                    char pieceChar = ' ';
                    switch (piece) {
                        case Pawn:
                            pieceChar = 'p';
                            break;
                        case Knight:
                            pieceChar = 'n';
                            break;
                        case Bishop:
                            pieceChar = 'b';
                            break;
                        case Rook:
                            pieceChar = 'r';
                            break;
                        case Queen:
                            pieceChar = 'q';
                            break;
                        case King:
                            pieceChar = 'k';
                            break;
                        default:
                            break;
                    }

                    // Determine color
                    Color color = White;
                    for (int c = White; c <= Black; ++c) {
                        if ((Bitboards[c][piece] & (1ULL << square)) != 0) {
                            color = static_cast<Color>(c);
                            break;
                        }
                    }

                    if (color == White) {
                        pieceChar = std::toupper(pieceChar);
                    }
                    fen << pieceChar;
                }
            }

            if (emptyCount > 0) {
                fen << emptyCount;
            }

            if (rank > 0) {
                fen << '/';
            }
        }

        // Side to move
        fen << ' ' << (SideToMove == White ? 'w' : 'b');

        // Castling rights
        fen << ' ';
        std::string castling;
        if (canCastle(WHITE_KINGSIDE))
            castling += 'K';
        if (canCastle(WHITE_QUEENSIDE))
            castling += 'Q';
        if (canCastle(BLACK_KINGSIDE))
            castling += 'k';
        if (canCastle(BLACK_QUEENSIDE))
            castling += 'q';
        if (castling.empty())
            castling = "-";
        fen << castling;

        // En passant square
        fen << ' ';
        if (currentState.EnpassantSquare == Null_Square) {
            fen << '-';
        } else {
            const int file = currentState.EnpassantSquare % 8;
            const int rank = currentState.EnpassantSquare / 8;
            fen << static_cast<char>('a' + file) << static_cast<char>('1' + rank);
        }

        // Fifty-move counter and full move number
        fen << ' ' << static_cast<int>(currentState.FiftyMoveCounter);
        fen << ' ' << getMoveCount();

        return fen.str();
    }

    void Board::pushState(const ChessMove &move, Piece capturedPiece) {
        // Copy current state and modify it
        StateHistory.emplace_back(getCurrentState());
        CurrentStateIndex++;

        auto &newState = getCurrentStateRef();
        newState.CastlingRights = StateHistory[CurrentStateIndex - 1].CastlingRights;
        newState.FiftyMoveCounter = StateHistory[CurrentStateIndex - 1].FiftyMoveCounter + 1;
        newState.EnpassantSquare = Null_Square;
        newState.CapturedPiece = capturedPiece;
        newState.LastMove = move;

        // Update for side to move change
        newState.Keys.updateSideToMove();

        // Update for en passant square change
        newState.Keys.updateEnPassant(StateHistory[CurrentStateIndex - 1].EnpassantSquare, Null_Square);
    }

    void Board::popState() {
        if (CurrentStateIndex > 0) {
            StateHistory.pop_back();
            CurrentStateIndex--;
        }
    }

    void Board::makeMove(const ChessMove &move) {
        const Square from = move.getFrom();
        const Square to = move.getTo();
        const Piece movingPiece = getPiece(from);
        const Piece capturedPiece = getPiece(to);
        const Color us = SideToMove;
        const Color them = (us == White) ? Black : White;

        // Push new state
        pushState(move, capturedPiece);

        // Handle captures
        if (capturedPiece != Null_Piece) {
            removePiece(them, capturedPiece, to);
            getCurrentStateRef().FiftyMoveCounter = 0;
        }

        // Handle pawn moves (reset fifty move counter)
        if (movingPiece == Pawn) {
            getCurrentStateRef().FiftyMoveCounter = 0;
        }

        // Update castling rights
        updateCastlingRights(from);
        updateCastlingRights(to);

        // Handle special moves
        switch (move.getMoveType()) {
            case NORMAL:
                movePiece(us, movingPiece, from, to);
                break;

            case PROMOTION: {
                removePiece(us, Pawn, from);
                setPiece(us, move.getPromotionPiece(), to);
                break;
            }

            case EN_PASSANT: {
                movePiece(us, Pawn, from, to);
                const Square capturedPawnSquare = static_cast<Square>(to + (us == White ? -8 : 8));
                removePiece(them, Pawn, capturedPawnSquare);
                break;
            }

            case CASTLING: {
                movePiece(us, King, from, to);
                // Move rook
                if (to > from) { // Kingside
                    const Square rookFrom = static_cast<Square>(us == White ? H1 : H8);
                    const Square rookTo = static_cast<Square>(us == White ? F1 : F8);
                    movePiece(us, Rook, rookFrom, rookTo);
                } else { // Queenside
                    const Square rookFrom = static_cast<Square>(us == White ? A1 : A8);
                    const Square rookTo = static_cast<Square>(us == White ? D1 : D8);
                    movePiece(us, Rook, rookFrom, rookTo);
                }
                break;
            }
        }

        // Handle en passant square setting for pawn double moves
        if (movingPiece == Pawn && std::abs(to - from) == 16) {
            const Square enpassantSquare = static_cast<Square>(from + (us == White ? 8 : -8));
            setEnpassantSquare(enpassantSquare);
        }

        // Switch sides
        SideToMove = them;

        // Update board state
        updateBoardState();
    }

    void Board::unmakeMove(const ChessMove &move) {
        // Switch sides back
        SideToMove = (SideToMove == White) ? Black : White;

        const Square from = move.getFrom();
        const Square to = move.getTo();
        const Color us = SideToMove;
        const Color them = (us == White) ? Black : White;
        const Piece capturedPiece = getCurrentState().CapturedPiece;

        // Handle special moves in reverse
        switch (move.getMoveType()) {
            case NORMAL: {
                const Piece movingPiece = getPiece(to);
                movePiece(us, movingPiece, to, from);
                break;
            }

            case PROMOTION: {
                removePiece(us, move.getPromotionPiece(), to);
                setPiece(us, Pawn, from);
                break;
            }

            case EN_PASSANT: {
                movePiece(us, Pawn, to, from);
                const Square capturedPawnSquare = static_cast<Square>(to + (us == White ? -8 : 8));
                setPiece(them, Pawn, capturedPawnSquare);
                break;
            }

            case CASTLING: {
                movePiece(us, King, to, from);
                // Move rook back
                if (to > from) { // Kingside
                    const Square rookFrom = static_cast<Square>(us == White ? F1 : F8);
                    const Square rookTo = static_cast<Square>(us == White ? H1 : H8);
                    movePiece(us, Rook, rookFrom, rookTo);
                } else { // Queenside
                    const Square rookFrom = static_cast<Square>(us == White ? D1 : D8);
                    const Square rookTo = static_cast<Square>(us == White ? A1 : A8);
                    movePiece(us, Rook, rookFrom, rookTo);
                }
                break;
            }
        }

        // Restore captured piece
        if (capturedPiece != Null_Piece) {
            setPiece(them, capturedPiece, to);
        }

        // Pop state
        popState();

        // Update board state
        updateBoardState();
    }

    void Board::makeNullMove() {
        const Color them = (SideToMove == White) ? Black : White;

        // Push new state
        pushState(ChessMove::nullMove(), Null_Piece);

        // Switch sides
        SideToMove = them;

        // Update board state
        updateBoardState();
    }

    void Board::unmakeNullMove() {
        // Switch sides back
        SideToMove = (SideToMove == White) ? Black : White;

        // Pop state
        popState();

        // Update board state
        updateBoardState();
    }

} // namespace motor
