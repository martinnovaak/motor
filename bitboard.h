#ifndef MOTOR_BITBOARD_H
#define MOTOR_BITBOARD_H

#include "bits.h"
#include <unordered_map>
#include <tuple>

enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    N_SQUARES = 64
};

enum Color : int {
    WHITE = 0,
    BLACK = 1,
    N_COLORS = 2
};

enum PieceType : int {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    N_PIECE_TYPES
};

static char Pieces[] {
    'P','N','B','R','Q', 'K'
};

enum Rank {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    N_RANKS
};

enum File {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    N_FILES
};

constexpr static uint64_t files[] {
        0x101010101010101ull,
        0x202020202020202ull,
        0x404040404040404ull,
        0x808080808080808ull,
        0x1010101010101010ull,
        0x2020202020202020ull,
        0x4040404040404040ull,
        0x8080808080808080ull
};

constexpr static uint64_t ranks[] {
        0xFFull,
        0xFF00ull,
        0xFF0000ull,
        0xFF000000ull,
        0xFF00000000ull,
        0xFF0000000000ull,
        0xFF000000000000ull,
        0xFF00000000000000ull
};

constexpr static uint64_t not_penultimate_ranks[] {
        0xFF00FFFFFFFFFFFFull,
        0xFFFFFFFFFFFF00FFull
};

constexpr static uint64_t full_board = 0xffffffffffffffffull;

enum CastlingRight {
    CASTLE_WHITE_KINGSIDE = 1,
    CASTLE_WHITE_QUEENSIDE = 2,
    CASTLE_BLACK_KINGSIDE = 4,
    CASTLE_BLACK_QUEENSIDE = 8,
    NONE = 0
};

const std::unordered_map<char, CastlingRight> char_to_castling_right {
    {'K', CASTLE_WHITE_KINGSIDE}, {'Q', CASTLE_WHITE_QUEENSIDE},
    {'k', CASTLE_BLACK_KINGSIDE}, {'q', CASTLE_BLACK_QUEENSIDE},
    {'-', NONE}
};

enum Direction : int {
                     NORTH_2 = 16,
    NORTH_WEST = 7,  NORTH = 8,  NORTH_EAST = 9,
    WEST = -1, N_DIRECTIONS = 8, EAST = 1,
    SOUTH_WEST = -9, SOUTH = -8, SOUTH_EAST = -7,
                     SOUTH_2 = - 16,
};

template<Direction direction>
constexpr uint64_t shift(uint64_t bitboard) {
    switch (direction) {
        case NORTH: return bitboard << 8;
        case SOUTH: return bitboard >> 8;
        case NORTH_2: return bitboard << 16;
        case SOUTH_2: return bitboard >> 16;
        case NORTH_EAST: return (bitboard & ~files[FILE_H]) << 9;
        case NORTH_WEST: return (bitboard & ~files[FILE_A]) << 7;
        case SOUTH_EAST: return (bitboard & ~files[FILE_H]) >> 7;
        case SOUTH_WEST: return (bitboard & ~files[FILE_A]) >> 9;
        case EAST: return (bitboard & ~files[FILE_H]) << 1;
        case WEST: return (bitboard & ~files[FILE_A]) >> 1;
    }
}


const std::unordered_map<std::string, Square> stringToSquare = {
        {"a1", A1}, {"b1", B1}, {"c1", C1}, {"d1", D1}, {"e1", E1}, {"f1", F1}, {"g1", G1}, {"h1", H1},
        {"a2", A2}, {"b2", B2}, {"c2", C2}, {"d2", D2}, {"e2", E2}, {"f2", F2}, {"g2", G2}, {"h2", H2},
        {"a3", A3}, {"b3", B3}, {"c3", C3}, {"d3", D3}, {"e3", E3}, {"f3", F3}, {"g3", G3}, {"h3", H3},
        {"a4", A4}, {"b4", B4}, {"c4", C4}, {"d4", D4}, {"e4", E4}, {"f4", F4}, {"g4", G4}, {"h4", H4},
        {"a5", A5}, {"b5", B5}, {"c5", C5}, {"d5", D5}, {"e5", E5}, {"f5", F5}, {"g5", G5}, {"h5", H5},
        {"a6", A6}, {"b6", B6}, {"c6", C6}, {"d6", D6}, {"e6", E6}, {"f6", F6}, {"g6", G6}, {"h6", H6},
        {"a7", A7}, {"b7", B7}, {"c7", C7}, {"d7", D7}, {"e7", E7}, {"f7", F7}, {"g7", G7}, {"h7", H7},
        {"a8", A8}, {"b8", B8}, {"c8", C8}, {"d8", D8}, {"e8", E8}, {"f8", F8}, {"g8", G8}, {"h8", H8},
        {"-", N_SQUARES}
};

const std::string squareToString[] = {
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
        "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
        "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
        "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
        "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

static std::unordered_map<Color, std::unordered_map<PieceType, char>> piece_to_char = []{
    auto map = decltype(piece_to_char){};
    map[WHITE][PAWN] = 'P';
    map[WHITE][KNIGHT] = 'N';
    map[WHITE][BISHOP] = 'B';
    map[WHITE][ROOK] = 'R';
    map[WHITE][QUEEN] = 'Q';
    map[WHITE][KING] = 'K';
    map[BLACK][PAWN] = 'p';
    map[BLACK][KNIGHT] = 'n';
    map[BLACK][BISHOP] = 'b';
    map[BLACK][ROOK] = 'r';
    map[BLACK][QUEEN] = 'q';
    map[BLACK][KING] = 'k';
    map[N_COLORS][N_PIECE_TYPES] = '.';
    return map;
}();

static std::unordered_map<char, std::pair<Color, PieceType>> charToPiece({
    {'P', {WHITE, PAWN}},  {'N', {WHITE, KNIGHT}}, {'B', {WHITE, BISHOP}},
    {'R', {WHITE, ROOK}}, {'Q', {WHITE, QUEEN}}, {'K', {WHITE, KING}},
    {'p', {BLACK, PAWN}}, {'n', {BLACK, KNIGHT}}, {'b', {BLACK, BISHOP}},
    {'r', {BLACK, ROOK}},  {'q', {BLACK, QUEEN}}, {'k', {BLACK, KING}},
    {'.', {N_COLORS, N_PIECE_TYPES}},
    });


#endif //MOTOR_BITBOARD_H
