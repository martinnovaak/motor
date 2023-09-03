#ifndef MOTOR_PERFT_HPP
#define MOTOR_PERFT_HPP

#include <chrono>
#include <iostream>

#include "chess_board/board.hpp"
#include "move_generation/move_generator.hpp"
#include "move_generation/move_list.hpp"

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
        b.make_move<side>(move);
        nodes += perft<next_side>(b, depth - 1);
        b.undo_move<side>();
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
    for (const auto & m : ml) {
        std::cout << m.to_string() << " ";
        b.make_move<side>(m);
        std::uint64_t nodes = perft<next_side>(b, depth - 1);
        std::cout << " " << nodes << "\n";
        total_nodes += nodes;
        b.undo_move<side>();
    }
    return total_nodes;
}

std::uint64_t perft_debug(board & b, int depth) {
    if (b.get_side() == White) {
        return perft_debug<White>(b, depth);
    } else {
        return perft_debug<Black>(b, depth);
    }
}

void bench() {
    board b1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // main position
    std::cout << "Main position\n";
    std::cout << "5: " << perft(b1, 5) << std::endl;
    std::cout << "6: " << perft(b1, 6) << std::endl;
    std::cout << "7: " << perft(b1, 7) << std::endl;

    board b2("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - "); // KIWIPETE
    std::cout << "\nKIWIPETE\n";
    std::cout << "4: " << perft(b2, 4) << std::endl;
    std::cout << "5: " << perft(b2, 5) << std::endl;
    std::cout << "6: " << perft(b2, 6) << std::endl;

    board b3("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ");
    std::cout << "\nPosition 3\n";
    std::cout << "5: " << perft(b3, 5) << std::endl;
    std::cout << "6: " << perft(b3, 6) << std::endl;
    std::cout << "7: " << perft(b3, 7) << std::endl;
    std::cout << "8: " << perft(b3, 8) << std::endl;

    board b4("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    std::cout << "\nPosition 4\n";
    std::cout << "5: " << perft(b4, 5) << std::endl;
    std::cout << "6: " << perft(b4, 6) << std::endl;

    board b5("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    std::cout << "\nPosition 5\n";
    std::cout << "4: " << perft(b5, 4) << std::endl;
    std::cout << "5: " << perft(b5, 5) << std::endl;

    board b6("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ");
    std::cout << "\nPosition 6\n";
    std::cout << "5: " << perft(b6, 5) << std::endl;
    std::cout << "6: " << perft(b6, 6) << std::endl;
}

#endif //MOTOR_PERFT_HPP
