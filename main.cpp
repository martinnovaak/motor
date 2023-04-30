#include "kgsb.h"
#include "movegen.h"
#include "movelist.h"
#include <chrono>
#include <iomanip>

uint64_t perft(board & b, int depth) {
    movelist ml;
    generate_moves(b, ml);

    if(depth == 1) {
        return ml.size();
    }

    uint64_t nodes = 0;
    for(move_t m : ml) {
        b.make_move(m);
        uint64_t node = perft(b, depth - 1);
        nodes += node;
        b.undo_move();
    }
    return nodes;
}

uint64_t perft_debug(board b, int depth) {
    movelist ml;
    generate_moves(b, ml);
    uint64_t total_nodes = 0;
    for(auto m : ml) {
        m.print_move();
        b.make_move(m);
        uint64_t nodes = perft(b, depth - 1);
        std::cout << " " << nodes << "\n";
        total_nodes += nodes;
        b.undo_move();
    }
    return total_nodes;
}

int main() 
{
    std::string fen;
    int depth = 5;

    std::cout << "FEN: ";
    std::getline(std::cin, fen);

    std::cout << "Depth: ";
    std::cin >> depth;

    std::cout << std::endl;

    board b(fen);
    auto start = std::chrono::steady_clock::now();
    uint64_t nodes = perft_debug(b, depth);
    std::cout << "\nNodes: " << nodes << "\n\n";
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_milliseconds = end - start;
    std::cout << "Time: " << elapsed_milliseconds << "\n";
    std::cout << std::fixed << std::setprecision(0) << "NPS: "  << round(nodes / elapsed_milliseconds.count()) * 1000 << "\n";

#if defined(_WIN32) || defined(_WIN64)
    system("pause");
#else
    cout << "Press any key to exit...";
    cin.get();
#endif
    /*
    board b ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    auto start = std::chrono::steady_clock::now();
    std::cout << perft_debug(b, 5) << "\n";
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "Time: " << elapsed_seconds.count() << "s\n";
    */
    
    return 0;
}
