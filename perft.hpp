#ifndef MOTOR_PERFT_HPP
#define MOTOR_PERFT_HPP

#include <chrono>
#include <iostream>
#include <algorithm>

#include "chess_board/board.hpp"
#include "move_generation/move_generator.hpp"
#include "move_generation/move_list.hpp"
#include "evaluation/evaluation.hpp"

template <Color side>
std::uint64_t perft(board& b, int depth) {
    constexpr Color next_side = side == White ? Black : White;
    move_list ml;
    generate_all_moves<side, false>(b, ml);

    if (depth == 1) {
        return ml.size();
    }

    std::uint64_t nodes = 0;
    for (const auto move : ml) {
        make_move<side, false>(b, move);
        nodes += perft<next_side>(b, depth - 1);
        undo_move<side, false>(b);
    }
    return nodes;
}

std::uint64_t perft(board & b, int depth) {
    auto start = std::chrono::steady_clock::now();
    std::uint64_t res = b.get_side() == White ? perft<White>(b, depth) : perft<Black>(b, depth);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
    std::cout << "Time: " << elapsed_milliseconds.count() << "  ";
    return res;
}

template <Color side>
std::uint64_t perft_debug(board & b, int depth) {
    constexpr Color next_side = (side == White) ? Black : White;
    move_list ml;
    generate_all_moves<side, false>(b, ml);
    std::uint64_t total_nodes = 0;
    std::vector<std::string> moves;
    for (const auto & m : ml) {
        std::string move_string = m.to_string();
        move_string += " ";
        make_move<side, false>(b, m);
        std::uint64_t nodes = perft<next_side>(b, depth - 1);
        moves.push_back(move_string += std::to_string(nodes));
        total_nodes += nodes;
        undo_move<side, false>(b);
    }
    std::sort(moves.begin(), moves.end());
    for (auto m : moves) {
        std::cout << m << "\n";
    }

    return total_nodes;
}

void perft_debug(board & b, int depth) {
    auto start = std::chrono::steady_clock::now();
    std::uint64_t result = b.get_side() == White ? perft_debug<White>(b, depth) : perft_debug<Black>(b, depth);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
    std::cout << "Time: " << elapsed_milliseconds.count() << ", nodes: " << result;
}

void perft_loop(board chessboard, int from, int to) {
    while (from <= to) {
        std::cout << from << ": " << perft(chessboard, from) << std::endl;
        from++;
    }
}

void perft_bench() {
    std::cout << "POSITION | NUMBER OF NODES | TIME IN MILLISECONDS\n";
    board chessboard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // main position
    std::cout << "Main position\n";
    perft_loop(chessboard, 1, 7);

    chessboard.fen_to_board("rnbqkbnr/ppppp1pp/8/5p2/4P3/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 2"); // main position
    std::cout << "\nMain position e2e4 f7f5\n";
    perft_loop(chessboard, 1, 6);

    chessboard.fen_to_board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - "); // KIWIPETE
    std::cout << "\nKIWIPETE\n";
    perft_loop(chessboard, 1, 6);

    chessboard.fen_to_board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ");
    std::cout << "\nPosition 3\n";
    perft_loop(chessboard, 1, 8);

    chessboard.fen_to_board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    std::cout << "\nPosition 4\n";
    perft_loop(chessboard, 1, 6);

    chessboard.fen_to_board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    std::cout << "\nPosition 5\n";
    perft_loop(chessboard, 1, 5);

    chessboard.fen_to_board("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ");
    std::cout << "\nPosition 6\n";
    perft_loop(chessboard, 1, 6);

    chessboard.fen_to_board("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1");
    std::cout << "\nPosition for promotion bugs\n";
    perft_loop(chessboard, 1, 6);
}

#endif //MOTOR_PERFT_HPP
