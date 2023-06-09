#ifndef MOTOR_BITS_H
#define MOTOR_BITS_H

#include <cstdint>

static bool get_bit(uint64_t bitboard, int square) {
    uint64_t mask = 1ull << square; // create a mask with a 1 at the i-th bit
    return bitboard & mask; // return true if the i-th bit is 1, false otherwise
}

static void set_bit(uint64_t& bb, int sq) {
    bb |= (1ULL << sq);
}

static void pop_bit(uint64_t& bb, int sq) {
    bb &= ~(1ULL << sq);
}

#if defined(__GNUC__) // GCC, Clang, ICC

static int lsb(uint64_t bitboard)
{
    return __builtin_ctzll(bitboard);
}

static int msb(uint64_t bitboard)
{
    return 63 ^ __builtin_clzll(bitboard);
}

#elif defined(_MSC_VER) // MSVC
#ifdef _WIN64 // MSVC, WIN64
#include <intrin.h>
static int lsb(uint64_t b)
{
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return idx;
}
static int msb(uint64_t b)
{
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return idx;
}
#endif
#else
#error "Compiler not supported."
#endif

static uint8_t popcount(uint64_t mask)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
    return (uint8_t)_mm_popcnt_u64(mask);
#else
    return __builtin_popcountll(mask);
#endif
}

constexpr static uint64_t pop_bits(uint64_t bitboard, int square1, int square2) {
    bitboard &= ~(1ULL << (square1));
    return bitboard & ~(1ULL << (square2));
}

static int pop_lsb(uint64_t & bitboard) {
    const int index = lsb(bitboard);
    bitboard &= bitboard - 1;
    return index;
}

#endif //MOTOR_BITS_H