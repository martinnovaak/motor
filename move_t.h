#ifndef MOTOR_MOVE_T_H
#define MOTOR_MOVE_T_H

#include <cstdint>
#include <iostream>
#include <bitset>

enum MoveType : int {
    QUIET                    = 0,
    DOUBLE_PAWN_PUSH         = 1,
    KING_CASTLE              = 2,
    QUEEN_CASTLE             = 3,
    CAPTURE                  = 4,
    EN_PASSANT               = 5,
    KNIGHT_PROMOTION         = 8,
    BISHOP_PROMOTION         = 9,
    ROOK_PROMOTION           = 10,
    QUEEN_PROMOTION          = 11,
    KNIGHT_PROMOTION_CAPTURE = 12,
    BISHOP_PROMOTION_CAPTURE = 13,
    ROOK_PROMOTION_CAPTURE   = 14,
    QUEEN_PROMOTION_CAPTURE  = 15,
};

class move_t {
public:
    constexpr move_t() {
        m_move = {};
    }
    constexpr move_t(int from, int to, Color color, int moving_piece, int type_information, int captured_piece) {
        m_move = from | to << 6 | color << 12 | moving_piece << 13 | type_information << 16 | captured_piece << 20;
    }

    constexpr move_t(int from, int to, Color color, int moving_piece, int type_information) {
        m_move = from | to << 6 | color << 12 | moving_piece << 13 | type_information << 16;
    }

    constexpr move_t(int from, int to, Color color, int moving_piece) {
        m_move = from | to << 6 | color << 12 | moving_piece << 13;
    }

    constexpr Square get_from() const {
        return Square(m_move & 63);
    }

    constexpr Square get_to() const {
        return Square(m_move >> 6 & 63);
    }

    constexpr Color get_color() const {
        return Color(m_move >> 12 & 1);
    }

    constexpr PieceType get_piece() const {
        return PieceType(m_move >> 13 & 7);
    }

    constexpr MoveType get_move_type() const {
        return MoveType(m_move >> 16 & 15);
    }

    constexpr PieceType get_captured_piece() const {
        return PieceType(m_move >> 20 & 7);
    }

    void print() const {
        std::cout << get_from() << " " << get_to() << " " << get_color() << " " << get_piece() <<" " << get_move_type() << " " << get_captured_piece();
    }

    void print_move() const {
        std::cout << Pieces[get_piece()] << squareToString[get_from()] << squareToString[get_to()] <<" ";
    }
private:
    uint32_t m_move;
};

#endif //MOTOR_MOVE_T_H
