#ifndef MOTOR_MOVE_T_H
#define MOTOR_MOVE_T_H

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

struct move_t {
public:
    [[nodiscard]] move_t() : m_move{} {}

    [[nodiscard]] move_t(int from, int to, Color color, int moving_piece, int type_information, int captured_piece) {
        m_move = from | to << 6 | color << 12 | moving_piece << 13 | type_information << 16 | captured_piece << 20;
    }

    [[nodiscard]] move_t(int from, int to, Color color, int moving_piece, int type_information) {
        m_move = from | to << 6 | color << 12 | moving_piece << 13 | type_information << 16 | KING << 20;
    }

    [[nodiscard]] move_t(int from, int to, Color color, int moving_piece) {
        constexpr int quiet_non_capture = QUIET << 16 | KING << 20;
        m_move = from | to << 6 | color << 12 | moving_piece << 13 | quiet_non_capture;
    }

    [[nodiscard]] Square get_from() const {
        return Square(m_move & 0b111111);
    }

    [[nodiscard]] Square get_to() const {
        return Square(m_move >> 6 & 0b111111);
    }

    [[nodiscard]] Color get_color() const {
        return Color(m_move >> 12 & 0b1);
    }

    [[nodiscard]] PieceType get_piece() const {
        return PieceType(m_move >> 13 & 0b111);
    }

    [[nodiscard]] MoveType get_move_type() const {
        return MoveType(m_move >> 16 & 0b1111);
    }

    [[nodiscard]] PieceType get_captured_piece() const {
        return PieceType(m_move >> 20 & 0b111);
    }

    [[nodiscard]] bool is_promotion() const {
        return get_move_type() > MoveType::EN_PASSANT;
    }

    [[nodiscard]] bool is_queen_promotion() const {
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
            case QUEEN_PROMOTION_CAPTURE:
            case QUEEN_PROMOTION:
                return true;
        }
    }

    [[nodiscard]] bool is_capture() const {
        switch (get_move_type()) {
            case QUIET:
            case DOUBLE_PAWN_PUSH:
            case KING_CASTLE:
            case QUEEN_CASTLE:
            case KNIGHT_PROMOTION:
            case BISHOP_PROMOTION:
            case ROOK_PROMOTION:
            case QUEEN_PROMOTION:
                return false;
            case CAPTURE:
            case EN_PASSANT:
            case KNIGHT_PROMOTION_CAPTURE:
            case BISHOP_PROMOTION_CAPTURE:
            case ROOK_PROMOTION_CAPTURE:
            case QUEEN_PROMOTION_CAPTURE:
                return true;
        }
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

    [[nodiscard]] std::string to_string() const {
        std::string move_string = "";
        move_string.append(square_to_string[get_from()]);
        move_string.append(square_to_string[get_to()]);
        MoveType mt = get_move_type();
        switch (mt)
        {
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

    [[nodiscard]] bool operator==(const move_t & other_move) const {
        return (this->m_move & 0x7fffffull) == (other_move.m_move & 0x7fffffull);
        //return m_move == other_move.m_move;
    }

    void set_score(int score) {
        m_move |= (score << 23);
        //m_score = score;
    }

    uint32_t m_move;
    //int m_score;
};

#endif //MOTOR_MOVE_T_H