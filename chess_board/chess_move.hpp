#ifndef MOTOR_CHESS_MOVE_HPP
#define MOTOR_CHESS_MOVE_HPP

#include "types.hpp"
#include "fen_utilities.hpp"

#include <cstdint>

const std::string square_to_string[] = {
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
        "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
        "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
        "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
        "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

enum MoveType : std::uint16_t {
    NORMAL,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING  = 3 << 14
};

enum PromotionType : std::uint16_t {
    KnightPromotion = 0,
    BishopPromotion = 1 << 12,
    RookPromotion = 2 << 12,
    QueenPromotion  = 3 << 12
};

class chess_move {
public:
    constexpr chess_move() : packed_move_data{} {}

    constexpr chess_move(Square from, Square to, MoveType move_type, PromotionType pt = KnightPromotion) {
        packed_move_data = static_cast<std::uint16_t>(move_type) | from | to << 6 | pt;
    }

    [[nodiscard]] Square get_from() const {
        return static_cast<Square>((packed_move_data) & 0b111111);
    }

    [[nodiscard]] Square get_to() const {
        return static_cast<Square>((packed_move_data >> 6) & 0b111111);
    }

    [[nodiscard]] MoveType get_move_type() const {
        return static_cast<MoveType>(packed_move_data & (3 << 14));
    }

    [[nodiscard]] Piece get_promotion() const {
        return static_cast<Piece>(((packed_move_data >> 12) & 3) + Knight);
    }

    bool operator==(const chess_move & other_move) const {
        return other_move.packed_move_data == packed_move_data;
    }

    [[nodiscard]] std::string to_string() const {
        std::string move_string;
        move_string.append(square_to_string[get_from()]);
        move_string.append(square_to_string[get_to()]);
        if (get_move_type() == PROMOTION)
        {
            switch(get_promotion()) {
                case Knight:
                    return move_string + "n";
                case Bishop:
                    return move_string + "b";
                case Rook:
                    return move_string + "r";
                case Queen:
                    return move_string + "q";
                default:
                    return move_string;
            }
        }
        return move_string;
    }

    [[nodiscard]] std::uint16_t get_value() const {
        return packed_move_data;
    }
private:
    std::uint16_t packed_move_data = 0;
};

#endif //MOTOR_CHESS_MOVE_HPP