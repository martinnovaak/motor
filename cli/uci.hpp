#ifndef MOTOR_UCI_HPP
#define MOTOR_UCI_HPP

#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>

#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../executioner/makemove.hpp"
#include "../search/time_keeper.hpp"
#include "../search/search.hpp"
#include "../search/bench.hpp"
#include "../perft.hpp"

std::vector<TuningOption*> tuning_options = {
        &pawn_weight, &nonpawn_weight, &threat_weight, &minor_weight, &concorrde_weight,
};

void print_tune_options() {
    std::cout << "name,type,default,min,max,step,0.002" << std::endl;
    for (const auto& option : tuning_options) {
        std::cout << option->get_name() << ",int,"
                  << option->value << ","
                  << option->min_value << ","
                  << option->max_value << ","
                  << 40 << ",0.002" << std::endl;
    }
}

bool parse_move(board & b, const std::string& move_string) {
    move_list ml;
    if (b.get_side() == White) {
        generate_all_moves<White, false>(b, ml);
    } else {
        generate_all_moves<Black, false>(b, ml);
    }

    for (const chess_move & m : ml) {
        if (m.to_string() == move_string) {
            if (b.get_side() == White) {
                make_move<White, false>(b, m);
            } else {
                make_move<Black, false>(b, m);
            }
            return true;
        }
    }
    return false;
}

void position_uci(board & b, const std::string & command) {
    // Initialize from FEN string
    int fen_pos = command.find("fen");
    int moves_pos = command.find("moves");

    if (command.find("startpos") != std::string::npos) {
        b.fen_to_board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    } else if (command.find("fen") != std::string::npos) {
        std::string fen = command.substr(fen_pos + 3, moves_pos);
        b.fen_to_board(fen);
    }

    if (moves_pos == std::string::npos) {
        set_position(b);
        return;
    }


    std::stringstream move_ss(command.substr(moves_pos + 5));
    std::vector<std::string> moves;
    std::string string_move;

    while (move_ss >> string_move) {
        moves.push_back(string_move);
    }

    int i = 0;
    for (const std::string & m : moves) {
        if (!parse_move(b, m)) return;
        i++;
        if (i > 200) {
            b.shift_history();
            i -= 100;
        }
    }

    set_position(b);
}

void uci_go(board& b, const std::string& command) {
    std::stringstream ss(command);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    time_info info;

    for (unsigned int i = 0; i < tokens.size(); i += 2) {
        if (tokens[i] == "wtime") {
            info.wtime = std::max(10, std::stoi(tokens[i + 1]));
        } else if (tokens[i] == "btime") {
            info.btime = std::max(10, std::stoi(tokens[i + 1]));
        } else if (tokens[i] == "winc") {
            info.winc = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "binc") {
            info.binc = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "movestogo") {
            info.movestogo = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "depth") {
            info.max_depth = std::stoi(tokens[i + 1]);
        } else if (tokens[i] == "movetime") {
            // info.movetime = std::stoi(tokens[i + 1]);  // NOT SUPPORTED RIGHT NOW
        } else if (tokens[i] == "infinite") {
            // info.infinite = true;
        } else if (tokens[i] == "nodes") {
            info.max_nodes = std::stoi(tokens[i + 1]);
        }
    }

    find_best_move(b, info);
}

void uci_process(board& b, const std::string& line) {
    std::stringstream ss(line);
    std::string command;

    ss >> command;

    if (command == "position") {
        position_uci(b, line.substr(9));
    } else if (command == "go") {
        uci_go(b, line.substr(3));
    } else if (command == "exit" || command == "quit" || command == "end") {
        exit(0);
    } else if (command == "isready") {
        std::cout << "readyok" << std::endl;
    } else if (command == "uci") {
        std::cout << "id name Motor 0.8.0 " << std::endl;
        std::cout << "id author Martin Novak " << std::endl;
        std::cout << "option name Hash type spin default " << 32 << " min 1 max 1024" << std::endl;
        std::cout << "option name Threads type spin default 1 min 1 max 1" << std::endl;

        auto print_option = [](const TuningOption* option) {
            std::cout << "option name " << option->name
                      << " type " << "spin"
                      << " default " << option->value
                      << " min " << option->min_value
                      << " max " << option->max_value
                      << std::endl;
        };

        std::for_each(tuning_options.begin(), tuning_options.end(), print_option);

        std::cout << "uciok" << std::endl;
    } else if (command == "ucinewgame") {
        history->clear();
        tt.clear();
    } else if (command == "setoption") {
        std::string token;
        std::vector<std::string> tokens;

        while (ss >> token) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 4) {
            if (tokens[1] == "Hash" || tokens[1] == "hash") {
                tt.resize(std::stoi(tokens[3]) * 1024 * 1024);
            }
        } else {
            auto it = std::find_if(tuning_options.begin(), tuning_options.end(),
                                   [&](TuningOption* opt) {
                                       return opt->get_name() == tokens[1];
                                   });

            if (it != tuning_options.end()) {
                (*it)->set_value(std::stoi(tokens[3]));
            }
            else {
                std::cout << "Option not found." << std::endl;
            }
        }
    } else if (command == "bench") {
        history->clear();
        tt.clear();
        bench(13);
    } else if (command == "perft") {
        ss >> command;
        perft_debug(b, std::stoi(command));
    } else if (command == "tune") {
        print_tune_options();
    }
}

void uci_mainloop() {
    board chessboard;
    std::string line{};

    while (std::getline(std::cin, line)) {
        uci_process(chessboard, line);
    }
}

#endif //MOTOR_UCI_HPP