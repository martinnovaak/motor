#ifndef MOTOR_TYPES_HPP
#define MOTOR_TYPES_HPP

#include <cstdint>

constexpr static std::uint64_t full_board = 0xffffffffffffffffull;

enum Square : std::uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    Null_Square
};

enum Color : std::uint8_t {
    White = 0, Black = 1,
};

enum Direction : std::int8_t {
    NORTH_2    =  16,
    NORTH_EAST =   9,
    NORTH      =   8,
    NORTH_WEST =   7,
    EAST       =   1,
    WEST       = - 1,
    SOUTH_EAST = - 7,
    SOUTH      = - 8,
    SOUTH_WEST = - 9,
    SOUTH_2    = -16,
};


Square operator+(Square square, Direction direction) {
    return static_cast<Square>(square + static_cast<Square>(direction));
}

Square operator-(Square square, Direction direction) {
    return static_cast<Square>(square - static_cast<Square>(direction));
}

enum Piece : uint8_t {
    Pawn, Knight, Bishop, Rook, Queen, King, Null_Piece
};

enum Rank : std::uint8_t {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8,
};

enum File : std::uint8_t {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H,
};

constexpr static std::uint64_t files[] {
        0x101010101010101ull,
        0x202020202020202ull,
        0x404040404040404ull,
        0x808080808080808ull,
        0x1010101010101010ull,
        0x2020202020202020ull,
        0x4040404040404040ull,
        0x8080808080808080ull
};

constexpr static std::uint64_t ranks[] {
        0x00000000000000ffull,
        0x000000000000ff00ull,
        0x0000000000ff0000ull,
        0x00000000ff000000ull,
        0x000000ff00000000ull,
        0x0000ff0000000000ull,
        0x00ff000000000000ull,
        0xff00000000000000ull
};

enum CastlingRight : std::uint8_t {
    CASTLE_WHITE_KINGSIDE  = 1,
    CASTLE_WHITE_QUEENSIDE = 2,
    CASTLE_BLACK_KINGSIDE  = 4,
    CASTLE_BLACK_QUEENSIDE = 8,
};
constexpr std::uint64_t bb(Square s) { return (1ULL << s); }

constexpr Square CastlingRookFrom[16] = {
        Null_Square, // NO_CASTLING
        H1,   // WHITE_KING_SIDE = 1,
        A1,   // WHITE_QUEEN_SIDE = 2,
        Null_Square,
        H8,   // BLACK_KING_SIDE  = 4,
        Null_Square,Null_Square,Null_Square,
        A8    // BLACK_QUEEN_SIDE = 8,
};
constexpr Square CastlingRookTo[16] = {
        Null_Square, // NO_CASTLING
        F1,   // WHITE_KING_SIDE = 1,
        D1,   // WHITE_QUEEN_SIDE = 2,
        Null_Square,
        F8,   // BLACK_KING_SIDE  = 4,
        Null_Square,Null_Square,Null_Square,
        D8    // BLACK_QUEEN_SIDE = 8,
};

// Squares that need to be empty for castling
constexpr std::uint64_t CastlingPath[16] = {
        0ull,                            // NO_CASTLING
        bb(F1) | bb(G1),              // WHITE_KING_SIDE = 1,
        bb(D1) | bb(C1) | bb(B1),     // WHITE_QUEEN_SIDE = 2,
        0ull,
        bb(F8) | bb(G8),              // BLACK_KING_SIDE  = 4,
        0ull,0ull,0ull,
        bb(D8) | bb(C8) | bb(B8)   // BLACK_QUEEN_SIDE = 8,
};

// Squares that need not to be attacked by ennemy for castling
constexpr std::uint64_t CastlingKingPath[16] = {
        0ull,                             // NO_CASTLING
        bb(E1) | bb(F1) | bb(G1),  // WHITE_KING_SIDE = 1,
        bb(E1) | bb(D1) | bb(C1),  // WHITE_QUEEN_SIDE = 2,
        0ull,
        bb(E8) | bb(F8) | bb(G8),  // BLACK_KING_SIDE  = 4,
        0ull,0ull,0ull,
        bb(E8) | bb(D8) | bb(C8)   // BLACK_QUEEN_SIDE = 8,
};

#endif //MOTOR_TYPES_HPP
