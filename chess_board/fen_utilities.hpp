#ifndef MOTOR_FEN_UTILITIES_HPP
#define MOTOR_FEN_UTILITIES_HPP

#include <cstdint>
#include <tuple>
#include <string>
#include "types.hpp"

std::pair<Color, Piece> get_color_and_piece(char fen_char) {
    switch (fen_char) {
        case 'P':
            return {Color::White, Piece::Pawn};
        case 'R':
            return {Color::White, Piece::Rook};
        case 'N':
            return {Color::White, Piece::Knight};
        case 'B':
            return {Color::White, Piece::Bishop};
        case 'Q':
            return {Color::White, Piece::Queen};
        case 'K':
            return {Color::White, Piece::King};
        case 'p':
            return {Color::Black, Piece::Pawn};
        case 'r':
            return {Color::Black, Piece::Rook};
        case 'n':
            return {Color::Black, Piece::Knight};
        case 'b':
            return {Color::Black, Piece::Bishop};
        case 'q':
            return {Color::Black, Piece::Queen};
        case 'k':
            return {Color::Black, Piece::King};
        default:
            return {Color::White, Piece::Null_Piece};
    }
}

std::uint8_t char_to_castling_right(char fen_right) {
    switch (fen_right) {
        case 'K':
            return CastlingRight::CASTLE_WHITE_KINGSIDE;
        case 'Q':
            return CastlingRight::CASTLE_WHITE_QUEENSIDE;
        case 'k':
            return CastlingRight::CASTLE_BLACK_KINGSIDE;
        case 'q':
            return CastlingRight::CASTLE_BLACK_QUEENSIDE;
        case '-':
        default:
            return 0;
    }
}

Square square_from_string(const std::string &s) {
    if ('a' <= s[0] && s[0] <= 'z') {
        return static_cast<Square>((s[0] - 'a') + (s[1] - '1') * 8);
    } else if ('A' <= s[0] && s[0] <= 'Z') {
        return static_cast<Square>((s[0] - 'A') + (s[1] - '1') * 8);
    } else { // s[0] == '-'
        return Square::Null_Square;
    }
}

#endif //MOTOR_FEN_UTILITIES_HPP
