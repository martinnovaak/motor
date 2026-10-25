#ifndef MOTOR_UCI_HPP
#define MOTOR_UCI_HPP

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../chess_board/board.hpp"
#include "../move_generation/move_list.hpp"
#include "../move_generation/move_generator.hpp"
#include "../executioner/makemove.hpp"
#include "../search/time_keeper.hpp"
#include "../search/search.hpp"
#include "../search/bench.hpp"
#include "../search/thread_pool.hpp"
#include "../perft.hpp"

const std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::vector<TuningOption*> tuning_options = {
    &tm_expect_mul, &tm_mul, &tm_stability_const, &tm_stability_mul, &tm_node_const, &tm_node_mul,
};

std::vector<std::string> tokenize(const std::string& line) {
    std::stringstream ss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// value of the token after `index`, or `fallback` when missing/malformed
std::int64_t parse_number(const std::vector<std::string>& tokens, std::size_t index, std::int64_t fallback) {
    if (index + 1 >= tokens.size()) {
        return fallback;
    }
    try {
        return std::stoll(tokens[index + 1]);
    } catch (const std::exception&) {
        return fallback;
    }
}

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

bool parse_move(position& p, const std::string& move_string) {
    board& b = p.chessboard;
    move_list ml;
    if (b.get_side() == White) {
        generate_all_moves<White, false>(b, ml);
    } else {
        generate_all_moves<Black, false>(b, ml);
    }

    for (const chess_move & m : ml) {
        if (m.to_string() == move_string) {
            if (b.get_side() == White) {
                make_move<White, false>(p, m);
            } else {
                make_move<Black, false>(p, m);
            }
            return true;
        }
    }
    return false;
}

void uci_position(position& p, const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        return;
    }

    board& b = p.chessboard;

    std::size_t moves_index = tokens.size();
    for (std::size_t i = 1; i < tokens.size(); i++) {
        if (tokens[i] == "moves") {
            moves_index = i;
            break;
        }
    }

    if (tokens[1] == "startpos") {
        b.fen_to_board(start_fen);
    } else if (tokens[1] == "fen") {
        std::string fen;
        for (std::size_t i = 2; i < moves_index; i++) {
            fen += tokens[i];
            fen += ' ';
        }
        b.fen_to_board(fen);
    } else {
        return;
    }

    int move_count = 0;
    for (std::size_t i = moves_index + 1; i < tokens.size(); i++) {
        if (!parse_move(p, tokens[i])) {
            break;
        }
        move_count++;
        if (move_count > 200) {
            b.shift_history();
            move_count -= 100;
        }
    }

    set_position(p);
}

void uci_go(position& p, const std::vector<std::string>& tokens) {
    time_info info;

    for (std::size_t i = 1; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        if (token == "wtime") {
            info.wtime = std::max(10, static_cast<int>(parse_number(tokens, i, info.wtime)));
        } else if (token == "btime") {
            info.btime = std::max(10, static_cast<int>(parse_number(tokens, i, info.btime)));
        } else if (token == "winc") {
            info.winc = static_cast<int>(parse_number(tokens, i, 0));
        } else if (token == "binc") {
            info.binc = static_cast<int>(parse_number(tokens, i, 0));
        } else if (token == "movestogo") {
            info.movestogo = static_cast<int>(parse_number(tokens, i, 0));
        } else if (token == "depth") {
            info.max_depth = static_cast<int>(parse_number(tokens, i, info.max_depth));
        } else if (token == "movetime") {
            info.movetime = std::max(1, static_cast<int>(parse_number(tokens, i, 0)));
        } else if (token == "nodes") {
            info.max_nodes = static_cast<std::uint64_t>(std::max<std::int64_t>(1, parse_number(tokens, i, 0)));
        }
        // "infinite": wtime/btime stay -1, so the timekeeper runs without a limit
    }

    search_pool.start_search(p, info);
}

void uci_setoption(const std::vector<std::string>& tokens) {
    // setoption name <Name> value <Value>
    std::string name, value;
    std::size_t i = 1;
    for (; i < tokens.size() && tokens[i] == "name"; i++) {}
    for (; i < tokens.size() && tokens[i] != "value"; i++) {
        if (!name.empty()) {
            name += ' ';
        }
        name += tokens[i];
    }
    if (i + 1 < tokens.size()) {
        value = tokens[i + 1];
    }

    if (name.empty() || value.empty()) {
        return;
    }

    int number;
    try {
        number = std::stoi(value);
    } catch (const std::exception&) {
        return;
    }

    if (name == "Hash" || name == "hash") {
        search_pool.resize_hash(static_cast<std::uint64_t>(std::clamp(number, 1, 1024)) * 1024 * 1024);
    } else if (name == "Threads" || name == "threads") {
        search_pool.resize(std::clamp(number, 1, 256));
    } else {
        auto it = std::find_if(tuning_options.begin(), tuning_options.end(),
                               [&](TuningOption* opt) { return opt->get_name() == name; });
        if (it != tuning_options.end()) {
            (*it)->set_value(number);
        } else {
            std::cout << "Option not found." << std::endl;
        }
    }
}

void uci_id() {
    std::cout << "id name Motor 0.9.0 " << std::endl;
    std::cout << "id author Martin Novak " << std::endl;
    std::cout << "option name Hash type spin default " << 32 << " min 1 max 1024" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 256" << std::endl;

    for (const TuningOption* option : tuning_options) {
        std::cout << "option name " << option->name
                  << " type spin"
                  << " default " << option->value
                  << " min " << option->min_value
                  << " max " << option->max_value
                  << std::endl;
    }

    std::cout << "uciok" << std::endl;
}

// returns false when the engine should quit
bool uci_process(position& p, const std::string& line) {
    const std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) {
        return true;
    }
    const std::string& command = tokens[0];

    if (command == "position") {
        if (search_pool.is_searching()) return true;
        uci_position(p, tokens);
    } else if (command == "go") {
        if (search_pool.is_searching()) return true;
        uci_go(p, tokens);
    } else if (command == "stop") {
        search_pool.stop_searching();
    } else if (command == "exit" || command == "quit" || command == "end") {
        return false;
    } else if (command == "isready") {
        std::cout << "readyok" << std::endl;
    } else if (command == "uci") {
        uci_id();
    } else if (command == "ucinewgame") {
        if (search_pool.is_searching()) return true;
        search_pool.clear_histories();
        search_pool.clear_hash();
    } else if (command == "setoption") {
        if (search_pool.is_searching()) return true;
        uci_setoption(tokens);
    } else if (command == "bench") {
        if (search_pool.is_searching()) return true;
        search_pool.clear_hash();
        bench(13);
    } else if (command == "perft") {
        if (search_pool.is_searching()) return true;
        perft_debug(p, static_cast<int>(parse_number(tokens, 0, 1)));
    } else if (command == "tune") {
        print_tune_options();
    }

    return true;
}

void uci_mainloop() {
    auto pos = std::make_unique<position>();
    set_position(*pos);
    std::string line{};

    while (std::getline(std::cin, line)) {
        if (!uci_process(*pos, line)) {
            break;
        }
    }

    search_pool.shutdown();
}

#endif //MOTOR_UCI_HPP
