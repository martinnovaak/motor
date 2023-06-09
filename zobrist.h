#ifndef MOTOR_ZOBRIST_H
#define MOTOR_ZOBRIST_H

#include <random>
#include "bitboard.h"

uint64_t piece_keys[N_COLORS][N_PIECE_TYPES][N_SQUARES];
uint64_t enpassant_keys[N_SQUARES + 1];
uint64_t castling_keys[16];
uint64_t side_key;

void init_hash_keys() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution(0ULL, std::numeric_limits<uint64_t>::max());

    for (int color = 0; color <= 1; color++) {
        for (int piece = PAWN; piece <= KING; piece++) {
            for (int square = 0; square < N_SQUARES; square++) {
                piece_keys[color][piece][square] = distribution(generator);
            }
        }
    }

    for (int square = 0; square < N_SQUARES; square++) {
        enpassant_keys[square] = distribution(generator);
    }
    enpassant_keys[N_SQUARES] = 0;

    for (int square = 0; square < 16; square++) {
        castling_keys[square] = distribution(generator);
    }

    side_key = distribution(generator);
}

#endif //MOTOR_ZOBRIST_H
