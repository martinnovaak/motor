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

    for (const std::string & m : moves) {
        if (!parse_move(b, m)) return;
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
        std::cout << "id name Motor 0.5.0 " << std::endl;
        std::cout << "id author Martin Novak " << std::endl;    
        std::cout << "option name Hash type spin default " << 32 << " min 1 max 1024" << std::endl;
        std::cout << "option name Threads type spin default 1 min 1 max 1" << std::endl;
/*
        std::cout << "option name mvv_pawn type spin default "<< mvv[Pawn] <<" min -4000 max 2000" << std::endl;
        std::cout << "option name mvv_knight type spin default "<< mvv[Knight] <<" min -2000 max 4000" << std::endl;
        std::cout << "option name mvv_bishop type spin default "<< mvv[Bishop] <<" min -2000 max 4000" << std::endl;
        std::cout << "option name mvv_rook type spin default "<< mvv[Rook] <<" min -1000 max 7000" << std::endl;
        std::cout << "option name mvv_queen type spin default "<< mvv[Queen] <<" min 0 max 12000" << std::endl;
        std::cout << "option name mvv_promotion type spin default "<< mvv[Null_Piece] <<" min 0 max 12000" << std::endl;

        std::cout << "option name quiet_mul type spin default "<< quiet_mul <<" min 100 max 800" << std::endl;
        std::cout << "option name quiet_max type spin default "<< quiet_max <<" min 1000 max 8000" << std::endl;
        std::cout << "option name noisy_base type spin default "<< noisy_base <<" min -5000 max 5000" << std::endl;
        std::cout << "option name noisy_mul type spin default "<< noisy_mul <<" min 10 max 800" << std::endl;
        std::cout << "option name noisy_max type spin default "<< noisy_max <<" min 200 max 10000" << std::endl;
        std::cout << "option name noisy_gravity type spin default "<< noisy_gravity <<" min 512 max 32768" << std::endl;

        std::cout << "option name iir_depth type spin default "<< iir_depth <<" min 2 max 8" << std::endl;
        std::cout << "option name razoring type spin default "<< razoring <<" min 200 max 1000" << std::endl;
        std::cout << "option name razoring_depth type spin default "<< razoring_depth <<" min 2 max 8" << std::endl;
        std::cout << "option name rfp type spin default "<< rfp <<" min 80 max 300" << std::endl;
        std::cout << "option name rfp_depth type spin default "<< rfp_depth <<" min 5 max 12" << std::endl;
        std::cout << "option name nmp type spin default "<< nmp <<" min 1 max 5" << std::endl;
        std::cout << "option name nmp_div type spin default "<< nmp_div <<" min 1 max 6" << std::endl;
        std::cout << "option name nmp_depth type spin default "<< nmp_depth <<" min 2 max 5" << std::endl;
        std::cout << "option name lmp_base type spin default "<< lmp_base <<" min 2 max 6" << std::endl;
        std::cout << "option name fp_base type spin default "<< fp_base <<" min 50 max 250" << std::endl;
        std::cout << "option name fp_mul type spin default "<< fp_mul <<" min 100 max 700" << std::endl;
        std::cout << "option name fp_depth type spin default "<< fp_depth <<" min 5 max 10" << std::endl;
        std::cout << "option name see_quiet type spin default "<< see_quiet <<" min 40 max 120" << std::endl;
        std::cout << "option name see_noisy type spin default "<< see_noisy <<" min 20 max 80" << std::endl;
        std::cout << "option name see_depth type spin default "<< see_depth <<" min 4 max 10" << std::endl;

        std::cout << "option name se_mul type spin default "<< se_mul <<" min 5 max 30" << std::endl;
        std::cout << "option name se_depth type spin default "<< se_depth <<" min 5 max 10" << std::endl;
        std::cout << "option name se_depth_margin type spin default "<< se_depth_margin <<" min 2 max 5" << std::endl;
        std::cout << "option name double_margin type spin default "<< double_margin <<" min 10 max 50" << std::endl;
        std::cout << "option name double_exts type spin default "<< double_exts <<" min 2 max 20" << std::endl;

        std::cout << "option name lmr_depth type spin default "<< lmr_depth <<" min 2 max 5" << std::endl;
        std::cout << "option name lmr_quiet_history type spin default "<< lmr_quiet_history <<" min 8000 max 20000" << std::endl;
        std::cout << "option name asp_window type spin default "<< asp_window <<" min 10 max 40" << std::endl;
        std::cout << "option name asp_window_mul type spin default "<< asp_window_mul <<" min 8 max 32" << std::endl;
        std::cout << "option name asp_window_max type spin default "<< asp_window_max <<" min 300 max 1000" << std::endl;
        std::cout << "option name asp_depth type spin default "<< asp_depth <<" min 4 max 10" << std::endl;
        std::cout << "option name lmr type spin default "<< 550 <<" min 200 max 1000" << std::endl;
*/
        std::cout << "uciok" << std::endl;
    } else if (command == "ucinewgame") {
        history_table = {};
        continuation_table = {};
        capture_table = {};
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
            /*
            else if (tokens[1] == "mvv_pawn") {
                mvv[Pawn] = std::stoi(tokens[3]);
            } else if (tokens[1] == "mvv_knight") {
                mvv[Knight] = std::stoi(tokens[3]);
            } else if (tokens[1] == "mvv_bishop") {
                mvv[Bishop] = std::stoi(tokens[3]);
            } else if (tokens[1] == "mvv_rook") {
                mvv[Rook] = std::stoi(tokens[3]);
            } else if (tokens[1] == "mvv_queen") {
                mvv[Queen] = std::stoi(tokens[3]);
            } else if (tokens[1] == "mvv_promotion") {
                mvv[Null_Piece] = std::stoi(tokens[3]);
            } else if (tokens[1] == "noisy_base") {
                noisy_base = std::stoi(tokens[3]);
            } else if (tokens[1] == "quiet_mul") {
                quiet_mul = std::stoi(tokens[3]);
            } else if (tokens[1] == "quiet_max") {
                quiet_max = std::stoi(tokens[3]);
            } else if (tokens[1] == "noisy_mul") {
                noisy_mul = std::stoi(tokens[3]);
            } else if (tokens[1] == "noisy_max") {
                noisy_max = std::stoi(tokens[3]);
            } else if (tokens[1] == "noisy_gravity") {
                noisy_gravity = std::stoi(tokens[3]);
            } else if (tokens[1] == "iir_depth") {
                iir_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "razoring") {
                razoring = std::stoi(tokens[3]);
            } else if (tokens[1] == "razoring_depth") {
                razoring_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "rfp") {
                rfp = std::stoi(tokens[3]);
            } else if (tokens[1] == "rfp_depth") {
                rfp_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "nmp") {
                nmp = std::stoi(tokens[3]);
            } else if (tokens[1] == "nmp_div") {
                nmp_div = std::stoi(tokens[3]);
            } else if (tokens[1] == "nmp_depth") {
                nmp_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "lmp_base") {
                lmp_base = std::stoi(tokens[3]);
            } else if (tokens[1] == "fp_base") {
                fp_base = std::stoi(tokens[3]);
            } else if (tokens[1] == "fp_mul") {
                fp_mul = std::stoi(tokens[3]);
            } else if (tokens[1] == "fp_depth") {
                fp_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "see_quiet") {
                see_quiet = std::stoi(tokens[3]);
            } else if (tokens[1] == "see_noisy") {
                see_noisy = std::stoi(tokens[3]);
            } else if (tokens[1] == "see_depth") {
                see_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "se_mul") {
                se_mul = std::stoi(tokens[3]);
            } else if (tokens[1] == "se_depth") {
                se_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "se_depth_margin") {
                se_depth_margin = std::stoi(tokens[3]);
            } else if (tokens[1] == "double_margin") {
                double_margin = std::stoi(tokens[3]);
            } else if (tokens[1] == "double_exts") {
                double_exts = std::stoi(tokens[3]);
            } else if (tokens[1] == "lmr_depth") {
                lmr_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "lmr_quiet_history") {
                lmr_quiet_history = std::stoi(tokens[3]);
            } else if (tokens[1] == "asp_window") {
                asp_window = std::stoi(tokens[3]);
            } else if (tokens[1] == "asp_window_mul") {
                asp_window_mul = std::stoi(tokens[3]);
            } else if (tokens[1] == "asp_window_max") {
                asp_window_max = std::stoi(tokens[3]);
            } else if (tokens[1] == "asp_depth") {
                asp_depth = std::stoi(tokens[3]);
            } else if (tokens[1] == "lmr") {
                lmr_table = initializeReductions(std::stoi(tokens[3]));
            }
             */
        } else {
            std::cout << "Command not found." << std::endl;
        }
    } else if (command == "bench") {
        history_table = {};
        continuation_table = {};
        capture_table = {};
        tt.clear();
        bench(13);
    } else if (command == "perft") {
        ss >> command;
        perft_debug(b, std::stoi(command));
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