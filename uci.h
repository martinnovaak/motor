#ifndef MOTOR_UCI_H
#define MOTOR_UCI_H

#include <fstream>
#include <iomanip>
#include <cmath>
#include "search.h"
#include "perft.h"

void go_perft(board &b, int depth) {
    auto start = std::chrono::steady_clock::now();
    uint64_t nodes = perft_debug(b, depth);
    std::cout << "\nNodes: " << nodes << "\n\n";
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
    std::cout << "Time: " << elapsed_milliseconds.count() << "\n";
    std::cout << std::fixed << std::setprecision(0) << "NPS: " << std::round(nodes / elapsed_milliseconds.count()) * 1000 << "\n";
}

bool parse_move(board & b, const std::string& move_string) {
    movelist ml;
    generate_moves(b, ml);

    for (const move_t& m : ml) {
        if (m.to_string() == move_string) {
            b.make_move(m);
            return true;
        }
    }
    return false;
}

void position_uci(board & b, const std::string & command) {
    // Initialize from FEN string
    int fen_pos = command.find("fen");
    int moves_pos = command.find("moves");

    if (fen_pos != std::string::npos) {
        std::string fen = command.substr(fen_pos + 3, moves_pos);
        b.fen_to_board(fen);
    }
    else if (command.find("kiwipete") != std::string::npos) {
        b.fen_to_board(KIWIPETE);
    }
    else if (command.find("startpos") != std::string::npos) {
        b.fen_to_board(START_POS);
    }

    if (moves_pos == std::string::npos) return;

    std::stringstream move_ss(command.substr(moves_pos + 5));
    std::vector<std::string> moves;
    std::string string_move;
    while (move_ss >> string_move) {
        moves.push_back(string_move);
    }

    for (const std::string& m : moves) {
        if (!parse_move(b, m)) return;
    }
}

void uci_go(board& b, const std::string& command) {
    int depth;
    std::stringstream ss(command);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    if (tokens[0] == "perft") {
        try { go_perft(b, std::stoi(tokens[1])); }
        catch (...) { go_perft(b, 6); }
        return;
    } else if(tokens[0] == "depth") {
        try { depth = std::stoi(tokens[1]); }
        catch (...) { depth = 6; }
        find_best_move(b, depth);
    }

    search_info info;

    for (unsigned int i = 0; i < tokens.size(); i += 2) {
        if (tokens[i] == "wtime") {
            info.wtime = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "btime") {
            info.btime = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "winc") {
            info.winc = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "binc") {
            info.binc = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "movestogo") {
            info.movestogo = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "depth") {
            info.max_depth = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "movetime") {
            info.movetime = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "infinite") {
            info.infinite = true;
        }

        find_best_move(b, info);
    }
}

void uci_process(board& b, const std::string& command) {
    // Parse the command string
    std::stringstream ss(command);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    if (tokens[0] == "position") {
        position_uci(b, command.substr(9));
    }
    else if (tokens[0] == "go") {
        uci_go(b, command.substr(3));
    }
    else if (tokens[0] == "bench") {
        bench();
    }
    else if (tokens[0] == "exit" || tokens[0] == "quit" || tokens[0] == "end") {
        exit(0);
    }
    else if (tokens[0] == "print" || tokens[0] == "printboard") {
        b.print_board();
    }
    else if (tokens[0] == "isready") {
        std::cout << "readyok" << std::endl;
    }
    else if (tokens[0] == "uci") {
        std::cout << "id name Motor " << 0 << "." << 0 << "." << 0 << std::endl;
        std::cout << "id author Martin Novak" << std::endl;
        std::cout << "uciok" << std::endl;
    }
    else if (tokens[0] == "ucinewgame") {
        position_uci(b, "startpos");
    }
}

void uci_mainloop() {
    board chessboard;
    std::string line{};

    std::ofstream outputFile("output.txt");

    while (std::getline(std::cin, line)) {
        outputFile << line << std::endl;
        uci_process(chessboard, line);
    }

    outputFile.close();
}

#endif //MOTOR_UCI_H
