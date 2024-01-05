#ifndef MOTOR_BITS_HPP
#define MOTOR_BITS_HPP

#include "types.hpp"

static void set_bit(std::uint64_t& bitboard, std::uint8_t square) {
    bitboard |= (1ull << square);
}

static void pop_bit(std::uint64_t& bitboard, std::uint8_t square) {
    bitboard &= ~(1ull << square);
}

static Square lsb(std::uint64_t bitboard)
{
    return static_cast<Square>(__builtin_ctzll(bitboard));
}

static uint8_t popcount(std::uint64_t mask)
{
    return __builtin_popcountll(mask);
}

static Square pop_lsb(std::uint64_t & bitboard) {
    const Square index = lsb(bitboard);
    bitboard &= bitboard - 1;
    return index;
}

static uint64_t pop_bits(uint64_t bitboard, int square1, int square2) {
    bitboard &= ~(1ULL << (square1));
    return bitboard & ~(1ULL << (square2));
}

#endif //MOTOR_BITS_HPP
