#pragma once
#include "board.h"
constexpr int piece_values[N_COLORS][N_PIECE_TYPES] = {
        {100, 300, 300, 500, 950 , 20'000}, {-100, -300, -300, -500, -950, -20'000}
};

static int evaluate(board& b) {
    int score = 0;

    for (int color = 0; color <= 1; color++) {
        for (int piece = 0; piece < 5; piece++) {
            uint64_t bitboard = b.get_pieces(color, piece);

            while (bitboard) {
                pop_lsb(bitboard);
                score += piece_values[color][piece] ;
            }
        }
    }

    return b.get_side() == WHITE ? score : -score;
}
