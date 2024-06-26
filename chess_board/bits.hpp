#ifndef MOTOR_BITS_HPP
#define MOTOR_BITS_HPP

#include <bit>
#include "types.hpp"

static Square lsb(std::uint64_t bitboard)
{
    return static_cast<Square>(std::countr_zero(bitboard));
}

static uint8_t popcount(std::uint64_t mask)
{
    return std::popcount(mask);
}

static Square pop_lsb(std::uint64_t & bitboard) {
    const Square index = lsb(bitboard);
    bitboard &= bitboard - 1;
    return index;
}

#endif //MOTOR_BITS_HPP
