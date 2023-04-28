#include "kgsb.h"
#include "movegen.h"
#include "movelist.h"
#include <chrono>

/*
// NOT USED ANYMORE
uint64_t generate_rook_mask(int square) {
    uint64_t mask = 0ULL;
    int row = square / 8;
    int col = square % 8;

    // Generate mask for the same row
    for (int i = 0; i < 8; i++) {
        if (i != col) {
            mask |= (1ULL << (row * 8 + i));
        }
    }

    // Generate mask for the same column
    for (int i = 0; i < 8; i++) {
        if (i != row) {
            mask |= (1ULL << (i * 8 + col));
        }
    }

    return mask;
}

uint64_t generate_bishop_mask(int square) {
    uint64_t mask = 0;
    int row = square / 8;
    int col = square % 8;
    int i, j;

    // northeast direction
    for (i = row + 1, j = col + 1; i <= 7 && j <= 7; i++, j++) {
        mask |= (1ULL << (i * 8 + j));
    }

    // northwest direction
    for (i = row - 1, j = col + 1; i >= 0 && j <= 7; i--, j++) {
        mask |= (1ULL << (i * 8 + j));
    }

    // southwest direction
    for (i = row - 1, j = col - 1; i >= 0 && j >= 0; i--, j--) {
        mask |= (1ULL << (i * 8 + j));
    }

    // southeast direction
    for (i = row + 1, j = col - 1; i <= 7 && j >= 0; i++, j--) {
        mask |= (1ULL << (i * 8 + j));
    }

    return mask;
}

 void generate_slider_masks() {
    for(int i = 0; i < 64; i++){
        rook_mask[i] = generate_rook_mask(i);
        bishop_mask[i] = generate_bishop_mask(i);
    }
}
*/

uint64_t perft(board & b, int depth) {
    movelist ml;
    generate_moves(b, ml);

    if(depth == 1) {
        return ml.size();
    }

    uint64_t nodes = 0;
    for(move m : ml) {
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
    for(auto m : ml) {
        m.print_move();
        b.make_move(m);
        std::cout << " " << perft(b, depth - 1) << "\n";
        b.undo_move();
    }
}

int main() {
    KGSSB::Init();

    board b("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    //perft_debug(b, 5);
    auto start = std::chrono::steady_clock::now();
    std::cout << perft(b, 5) << "\n";
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

    return 0;
}
