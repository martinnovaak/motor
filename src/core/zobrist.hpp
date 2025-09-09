#ifndef MOTOR_ZOBRIST_HPP
#define MOTOR_ZOBRIST_HPP

#include <array>
#include <cstdint>
#include "types.hpp"

namespace zobrist_keys {

    class ConstexprRng {
    public:
        constexpr explicit ConstexprRng(std::uint64_t Seed = 0x853c49e6748fea9bULL) : State(Seed) {}

        constexpr std::uint64_t next() {
            State ^= State >> 12;
            State ^= State << 25;
            State ^= State >> 27;
            return State * 0x2545f4914f6cdd1dULL;
        }

    private:
        std::uint64_t State;
    };

    // Generate piece-square table keys
    constexpr auto generatePsqtKeys() {
        std::array<std::array<std::array<std::uint64_t, 64>, 6>, 2> Keys{};
        ConstexprRng Rng(0x853c49e6748fea9bULL);

        for (int Color = 0; Color < 2; ++Color) {
            for (int Piece = 0; Piece < 6; ++Piece) {
                for (int Square = 0; Square < 64; ++Square) {
                    Keys[Color][Piece][Square] = Rng.next();
                }
            }
        }
        return Keys;
    }

    // Generate en passant keys
    constexpr auto generateEnpassantKeys() {
        std::array<std::uint64_t, 65> Keys{};
        ConstexprRng Rng(0x9fb21c651e98df25ULL);

        for (int I = 0; I < 65; ++I) {
            Keys[I] = Rng.next();
        }
        Keys[64] = 0ULL; // No en passant square
        return Keys;
    }

    // Generate castling keys
    constexpr auto generateCastlingKeys() {
        std::array<std::uint64_t, 16> Keys{};
        ConstexprRng Rng(0xc4ceb9fe1a85ec53ULL);

        Keys[0] = 0ULL; // No castling rights
        for (int I = 1; I < 16; ++I) {
            Keys[I] = Rng.next();
        }
        return Keys;
    }

    // Generate side to move key
    constexpr std::uint64_t generateSideKey() {
        ConstexprRng Rng(0x1b873593c0e6e90fULL);
        return Rng.next();
    }

    // Generated keys
    constexpr auto PSQT_KEYS = generatePsqtKeys();
    constexpr auto ENPASSANT_KEYS = generateEnpassantKeys();
    constexpr auto CASTLING_KEYS = generateCastlingKeys();
    constexpr std::uint64_t SIDE_KEY = generateSideKey();

} // namespace zobrist_keys

class Zobrist {
public:
    constexpr Zobrist() : HashKey(0ULL) {}

    constexpr void updateCastlingHash(std::uint8_t Right) { HashKey ^= zobrist_keys::CASTLING_KEYS[Right]; }

    constexpr void updateSideHash() { HashKey ^= zobrist_keys::SIDE_KEY; }

    constexpr void updateEnpassantHash(Square EnpassantSquare) {
        HashKey ^= zobrist_keys::ENPASSANT_KEYS[EnpassantSquare];
    }

    constexpr void updatePsqtHash(Color C, Piece P, Square S) { HashKey ^= zobrist_keys::PSQT_KEYS[C][P][S]; }

    [[nodiscard]] constexpr std::uint64_t getKey() const { return HashKey; }

    constexpr bool operator==(const Zobrist &Other) const { return Other.HashKey == HashKey; }

private:
    std::uint64_t HashKey;
};

#endif // MOTOR_ZOBRIST_HPP
