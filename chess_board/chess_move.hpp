#ifndef MOTOR_CHESS_MOVE_HPP
#define MOTOR_CHESS_MOVE_HPP

#include "types.hpp"

#include <cstdint>

enum MoveType : std::uint16_t {
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

class chess_move {
public:
    chess_move() : move_data{} {}

    chess_move(Square from, Square to, MoveType move_type) {
        from | to << 6 | move_type << 12;
    }

    [[nodiscard]] Square get_from() const {
        return static_cast<Square>((move_data) & 0x0b111111);
    }

    [[nodiscard]] Square get_to() const {
        return static_cast<Square>((move_data >> 6) & 0x0b111111);
    }

    [[nodiscard]] MoveType get_move_type() const {
        return static_cast<MoveType>((move_data >> 12) & 0b1111);
    }
private:
    std::uint16_t move_data = 0;
};

constexpr chess_move null_move();

#endif //MOTOR_CHESS_MOVE_HPP
