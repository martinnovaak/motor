#include "uci.hpp"
#include "../benchmarks/perft.hpp"
#include <iostream>
#include <algorithm>

namespace motor {

    UCI::UCI() {
        coordinator = std::make_unique<Coordinator>();
        searcher = std::make_unique<Search>(coordinator.get());
    }

    void UCI::loop() {
        std::string input;

        while (std::getline(std::cin, input)) {
            if (input.empty()) continue;

            auto tokens = splitCommand(input);
            if (tokens.empty()) continue;

            const std::string& command = tokens[0];

            if (command == "uci") {
                handleUCI();
            } else if (command == "isready") {
                handleIsReady();
            } else if (command == "position") {
                handlePosition(input);
            } else if (command == "go") {
                handleGo(input);
            } else if (command == "stop") {
                handleStop();
            } else if (command == "quit") {
                handleQuit();
                break;
            } else if (command == "ucinewgame") {
                handleUCINewGame();
            } else if (command == "setoption") {
                handleSetOption(input);
            } else if (command == "eval") {
                handleEval();
            } else if (command == "d") {
                handleD();
            }
        }
    }

    void UCI::handleUCI() {
        sendResponse("id name " + std::string(ENGINE_NAME) + " " + std::string(ENGINE_VERSION));

        // Engine options
        sendResponse("option name Debug type check default false");
        sendResponse("option name Hash type spin default 64 min 1 max 1024");

        sendResponse("uciok");
    }

    void UCI::handleIsReady() {
        sendResponse("readyok");
    }

    void UCI::handlePosition(const std::string& command) {
        auto tokens = splitCommand(command);

        if (tokens.size() < 2) return;

        std::string fen;
        std::vector<std::string> moves;

        if (tokens[1] == "startpos") {
            fen = std::string(Board::STARTING_FEN);

            // Look for moves
            auto moves_it = std::find(tokens.begin(), tokens.end(), "moves");
            if (moves_it != tokens.end()) {
                moves.assign(moves_it + 1, tokens.end());
            }
        } else if (tokens[1] == "fen") {
            // Build FEN string
            std::ostringstream fen_stream;
            size_t i = 2;
            while (i < tokens.size() && tokens[i] != "moves") {
                if (i > 2) fen_stream << " ";
                fen_stream << tokens[i];
                i++;
            }
            fen = fen_stream.str();

            // Look for moves
            if (i < tokens.size() && tokens[i] == "moves") {
                moves.assign(tokens.begin() + i + 1, tokens.end());
            }
        }

        setupPosition(fen, moves);
    }

    void UCI::handleGo(const std::string& command) {
        auto tokens = splitCommand(command);
        SearchLimits limits;

        for (size_t i = 1; i < tokens.size(); i++) {
            const std::string& token = tokens[i];

            if (token == "depth" && i + 1 < tokens.size()) {
                limits.depth = std::stoi(tokens[++i]);
            } else if (token == "nodes" && i + 1 < tokens.size()) {
                limits.nodes = std::stoi(tokens[++i]);
            } else if (token == "movetime" && i + 1 < tokens.size()) {
                limits.movetime = std::stoi(tokens[++i]);
            } else if (token == "wtime" && i + 1 < tokens.size()) {
                limits.wtime = std::stoi(tokens[++i]);
            } else if (token == "btime" && i + 1 < tokens.size()) {
                limits.btime = std::stoi(tokens[++i]);
            } else if (token == "winc" && i + 1 < tokens.size()) {
                limits.winc = std::stoi(tokens[++i]);
            } else if (token == "binc" && i + 1 < tokens.size()) {
                limits.binc = std::stoi(tokens[++i]);
            } else if (token == "movestogo" && i + 1 < tokens.size()) {
                limits.movestogo = std::stoi(tokens[++i]);
            } else if (token == "infinite") {
                limits.infinite = true;
            } else if (token == "ponder") {
                limits.ponder = true;
            }
        }

        // Default depth if no limits specified
        if (limits.depth == 0 && limits.nodes == 0 && limits.movetime == 0 &&
            limits.wtime == 0 && limits.btime == 0 && !limits.infinite) {
            limits.depth = 5;  // Default search depth
        }

        SearchResult result = searcher->search(limits);

        std::string response = "bestmove " + result.best_move.toString();
        if (!result.ponder_move.isNull()) {
            response += " ponder " + result.ponder_move.toString();
        }
        sendResponse(response);
    }

    void UCI::handleStop() {
        searcher->stop();
    }

    void UCI::handleQuit() {
        // Cleanup if needed
    }

    void UCI::handleUCINewGame() {
        coordinator = std::make_unique<Coordinator>();
        searcher = std::make_unique<Search>(coordinator.get());
    }

    void UCI::handleSetOption(const std::string& command) {
        auto tokens = splitCommand(command);

        if (tokens.size() >= 5 && tokens[1] == "name" && tokens[3] == "value") {
            const std::string& name = tokens[2];
            const std::string& value = tokens[4];

            if (name == "Hash") {
                // TODO: Hash table size - implement when adding transposition table
                int hash_size = std::stoi(value);
            }
        }
    }

    void UCI::handleEval() {
        int eval = coordinator->evaluate();
        std::ostringstream response;
        response << "Evaluation: " << eval << " (from "
                 << (coordinator->getSideToMove() == White ? "White" : "Black") << "'s perspective)";
        sendResponse(response.str());
    }

    void UCI::handleD() {
        printBoard();
        sendResponse("FEN: " + coordinator->toFen());
        sendResponse("Evaluation: " + std::to_string(coordinator->evaluate()));
    }

    void UCI::sendResponse(const std::string& response) {
        std::cout << response << std::endl;
    }

    std::vector<std::string> UCI::splitCommand(const std::string& command) {
        std::vector<std::string> tokens;
        std::istringstream iss(command);
        std::string token;

        while (iss >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }

    void UCI::setupPosition(const std::string& fen, const std::vector<std::string>& moves) {
        coordinator->setFromFen(fen);

        for (const std::string& moveStr : moves) {
            ChessMove move = parseMove(moveStr);
            if (!move.isNull()) {
                coordinator->makeMove(move);
            }
        }
    }

    ChessMove UCI::parseMove(const std::string& moveStr) {
        if (moveStr.length() < 4) return ChessMove::nullMove();

        // Parse UCI move format (e.g., "e2e4", "e7e8q")
        const char fromFile = moveStr[0];
        const char fromRank = moveStr[1];
        const char toFile = moveStr[2];
        const char toRank = moveStr[3];

        if (fromFile < 'a' || fromFile > 'h' || fromRank < '1' || fromRank > '8' ||
            toFile < 'a' || toFile > 'h' || toRank < '1' || toRank > '8') {
            return ChessMove::nullMove();
        }

        const Square from = static_cast<Square>((fromFile - 'a') + (fromRank - '1') * 8);
        const Square to = static_cast<Square>((toFile - 'a') + (toRank - '1') * 8);

        // Check for promotion
        if (moveStr.length() == 5) {
            const char promotionChar = moveStr[4];
            Piece promotionPiece = Queen;  // Default

            switch (promotionChar) {
                case 'n': promotionPiece = Knight; break;
                case 'b': promotionPiece = Bishop; break;
                case 'r': promotionPiece = Rook; break;
                case 'q': promotionPiece = Queen; break;
            }

            return ChessMove::createPromotionMove(from, to, promotionPiece);
        }

        return ChessMove::createNormalMove(from, to);
    }

    void UCI::printBoard() {
        const Board& board = coordinator->getBoard();

        sendResponse("  +---+---+---+---+---+---+---+---+");
        for (int rank = 7; rank >= 0; --rank) {
            std::ostringstream row;
            row << (rank + 1) << " |";

            for (int file = 0; file < 8; ++file) {
                const Square square = static_cast<Square>(rank * 8 + file);
                const Piece piece = board.getPiece(square);

                char pieceChar = ' ';
                if (piece != Null_Piece) {
                    switch (piece) {
                        case Pawn: pieceChar = 'P'; break;
                        case Knight: pieceChar = 'N'; break;
                        case Bishop: pieceChar = 'B'; break;
                        case Rook: pieceChar = 'R'; break;
                        case Queen: pieceChar = 'Q'; break;
                        case King: pieceChar = 'K'; break;
                        default: pieceChar = '?'; break;
                    }

                    // Determine piece color
                    bool isWhite = false;
                    for (int color = White; color <= Black; ++color) {
                        if (board.getPieces(static_cast<Color>(color), piece) & (1ULL << square)) {
                            isWhite = (color == White);
                            break;
                        }
                    }

                    if (!isWhite) {
                        pieceChar = std::tolower(pieceChar);
                    }
                }

                row << " " << pieceChar << " |";
            }

            sendResponse(row.str());
            sendResponse("  +---+---+---+---+---+---+---+---+");
        }
        sendResponse("    a   b   c   d   e   f   g   h");
    }

} // namespace motor
