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

enum Check_type : uint8_t {
    NOCHECK         = 0b00,
    DIRECT_CHECK    = 0b01,
    DISCOVERY_CHECK = 0b10,
    DOUBLE_CHECK    = 0b11,
};

class chess_move {
public:
    chess_move() : move_data{} {}

    chess_move(Square from, Square to, MoveType move_type) {
        move_data = from | to << 6 | move_type << 12;
    }

    chess_move(Square from, Square to, MoveType move_type, bool direct_check, bool discovery) {
        move_data = from | to << 6 | move_type << 12 | direct_check << 16 | discovery << 17;
    }

    [[nodiscard]] Square get_from() const {
        return static_cast<Square>((move_data) & 0b111111);
    }

    [[nodiscard]] Square get_to() const {
        return static_cast<Square>((move_data >> 6) & 0b111111);
    }

    [[nodiscard]] MoveType get_move_type() const {
        return static_cast<MoveType>((move_data >> 12) & 0b1111);
    }

    [[nodiscard]] Check_type get_check_type() const {
        return static_cast<Check_type>((move_data >> 16) & 0b11);
    }

    bool operator==(const chess_move & other_move) const {
        return other_move.move_data == move_data;
    }

    bool operator>(const chess_move & other_move) const {
        return move_data > other_move.move_data;
    }

    std::uint32_t get_value() {
        return move_data;
    }

    [[nodiscard]] bool is_quiet() const {
        switch (get_move_type()) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
            case KING_CASTLE:
            case QUEEN_CASTLE:
                return true;
            case CAPTURE:
            case EN_PASSANT:
            case KNIGHT_PROMOTION:
            case BISHOP_PROMOTION:
            case ROOK_PROMOTION:
            case QUEEN_PROMOTION:
            case KNIGHT_PROMOTION_CAPTURE:
            case BISHOP_PROMOTION_CAPTURE:
            case ROOK_PROMOTION_CAPTURE:
            case QUEEN_PROMOTION_CAPTURE:
                return false;
        }
    }

    [[nodiscard]] bool is_capture() const {
        switch (get_move_type()) {
        case QUIET:
        case DOUBLE_PAWN_PUSH:
        case KING_CASTLE:
        case QUEEN_CASTLE:
        case EN_PASSANT:
        case KNIGHT_PROMOTION:
        case BISHOP_PROMOTION:
        case ROOK_PROMOTION:
        case QUEEN_PROMOTION:
            return false;
        case CAPTURE:
        case KNIGHT_PROMOTION_CAPTURE:
        case BISHOP_PROMOTION_CAPTURE:
        case ROOK_PROMOTION_CAPTURE:
        case QUEEN_PROMOTION_CAPTURE:
            return true;
        }
    }

    [[nodiscard]] bool is_promotion() const {
        switch (get_move_type()) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
            case KING_CASTLE:
            case QUEEN_CASTLE:
            case CAPTURE:
            case EN_PASSANT:
            case KNIGHT_PROMOTION:
            case BISHOP_PROMOTION:
            case ROOK_PROMOTION:
            case KNIGHT_PROMOTION_CAPTURE:
            case BISHOP_PROMOTION_CAPTURE:
            case ROOK_PROMOTION_CAPTURE:
                return false;
            case QUEEN_PROMOTION:
            case QUEEN_PROMOTION_CAPTURE:
                return true;
        }
    }

    [[nodiscard]] std::string to_string() const {
        std::string move_string;
        move_string.append(square_to_string[get_from()]);
        move_string.append(square_to_string[get_to()]);
        MoveType mt = get_move_type();
        switch (mt)
        {
            case KING_CASTLE:
                move_string = "";
                move_string.append(square_to_string[get_from() - 3]);
                move_string.append(square_to_string[get_to()   + 1]);
                return move_string;
            case QUEEN_CASTLE:
                move_string = "";
                move_string.append(square_to_string[get_from() + 4]);
                move_string.append(square_to_string[get_to()   - 1]);
                return move_string;
            case KNIGHT_PROMOTION:
                return move_string + "n";
            case BISHOP_PROMOTION:
                return move_string + "b";
            case ROOK_PROMOTION:
                return move_string + "r";
            case QUEEN_PROMOTION:
                return move_string + "q";
            case KNIGHT_PROMOTION_CAPTURE:
                return move_string + "n";
            case BISHOP_PROMOTION_CAPTURE:
                return move_string + "b";
            case ROOK_PROMOTION_CAPTURE:
                return move_string + "r";
            case QUEEN_PROMOTION_CAPTURE:
                return move_string + "q";
            default:
                return move_string;
        }
    }
private:
    std::uint32_t move_data = 0;
};

#endif //MOTOR_CHESS_MOVE_HPP