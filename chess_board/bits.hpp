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
#if defined(__GNUC__) // GCC, Clang, ICC
    return static_cast<Square>(__builtin_ctzll(bitboard));
#elif defined(_MSC_VER) // MSVC
    #ifdef _WIN64 // MSVC, WIN64
#include <intrin.h>
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return static_cast<Square>(idx);
#endif
#else
#error "Compiler not supported."
#endif
}

static uint8_t popcount(std::uint64_t mask)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
    return (uint8_t)_mm_popcnt_u64(mask);
#else
    return __builtin_popcountll(mask);
#endif
}

static int pop_lsb(std::uint64_t & bitboard) {
    const int index = lsb(bitboard);
    bitboard &= bitboard - 1;
    return index;
}

#endif //MOTOR_BITS_HPP
