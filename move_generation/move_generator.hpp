#ifndef MOTOR_MOVE_GENERATOR_HPP
#define MOTOR_MOVE_GENERATOR_HPP

#include "../chess_board/board.hpp"
#include "move_list.hpp"

template<Color side, bool in_check, bool only_captures>
void generate_promotions(const board & chessboard, std::uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr std::uint64_t penultimate_rank = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
    constexpr Direction up = (side == White) ? NORTH : SOUTH;
    constexpr Direction up_left = (side == White) ? NORTH_WEST : SOUTH_EAST;
    constexpr Direction up_right = (side == White) ? NORTH_EAST : SOUTH_WEST;

    const std::uint64_t empty = ~chessboard.get_occupancy();
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pins = data.orthogonal_pins;
    const std::uint64_t diagonal_pins = data.diagonal_pins;
    const std::uint64_t checkmask = data.checkmask;

    std::uint64_t pawns_penultimate = source & penultimate_rank;
    if (pawns_penultimate) {
        // Capture Promotion
        std::uint64_t pawns = pawns_penultimate & ~orthogonal_pins;
        std::uint64_t left_promotions = (shift<up_left>(pawns & ~diagonal_pins) | (shift<up_left>(pawns & diagonal_pins) & diagonal_pins)) & enemy;
        std::uint64_t right_promotions = (shift<up_right>(pawns & ~diagonal_pins) | (shift<up_right>(pawns & diagonal_pins) & diagonal_pins)) & enemy;

        if constexpr (in_check) {
            left_promotions &= checkmask;
            right_promotions &= checkmask;
        }

        while(left_promotions) {
            const Square to = pop_lsb(left_promotions);
            const Square from = to - up_left;
            moves.add(chess_move(from, to, QUEEN_PROMOTION_CAPTURE));

            if constexpr (!only_captures) {
                moves.add(chess_move(from, to, KNIGHT_PROMOTION_CAPTURE));
                moves.add(chess_move(from, to, ROOK_PROMOTION_CAPTURE));
                moves.add(chess_move(from, to, BISHOP_PROMOTION_CAPTURE));
            }
        }

        while(right_promotions) {
            const Square to = pop_lsb(right_promotions);
            const Square from = to - up_right;
            moves.add(chess_move(from, to, QUEEN_PROMOTION_CAPTURE));

            if constexpr (!only_captures) {
                moves.add(chess_move(from, to, KNIGHT_PROMOTION_CAPTURE));
                moves.add(chess_move(from, to, ROOK_PROMOTION_CAPTURE));
                moves.add(chess_move(from, to, BISHOP_PROMOTION_CAPTURE));
            }
        }

        // Quiet Promotion
        pawns = pawns_penultimate & ~diagonal_pins;
        std::uint64_t quiet_promotions = (shift<up>(pawns & ~orthogonal_pins) | (shift<up>(pawns & orthogonal_pins) & orthogonal_pins)) & empty;

        if constexpr (in_check) quiet_promotions &= checkmask;

        while(quiet_promotions) {
            const Square to = pop_lsb(quiet_promotions);
            const Square from = to - up;
            moves.add(chess_move(from, to, QUEEN_PROMOTION));

            if constexpr (!only_captures) {
                moves.add(chess_move(from, to, KNIGHT_PROMOTION));
                moves.add(chess_move(from, to, ROOK_PROMOTION));
                moves.add(chess_move(from, to, BISHOP_PROMOTION));
            }
        }
    }
}

template<Color side, bool in_check>
void generate_enpassant(const board &chessboard, std::uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr Direction pawn_direction = side == White ? NORTH : SOUTH;

    const Square ep_square = chessboard.enpassant_square();
    // Enpassant
    if (ep_square != Square::Null_Square) {
        std::uint64_t orthogonal_pins = data.orthogonal_pins;
        std::uint64_t diagonal_pins = data.diagonal_pins;
        std::uint64_t checkmask = data.checkmask;

        std::uint64_t capture = bb(chessboard.enpassant_square() - pawn_direction);
        std::uint64_t enpassants = PAWN_ATTACKS_TABLE[their_side][chessboard.enpassant_square()] & source & ~orthogonal_pins;

        if constexpr (in_check) {
            if (!(data.checkers & capture)) {
                enpassants &= checkmask;
            }
        }

        while(enpassants) {
            const Square from = pop_lsb(enpassants);

            if ((bb(from) & diagonal_pins) && !(bb(ep_square) & diagonal_pins)) {
                continue;
            }

            if (!(attacks<Ray::HORIZONTAL>(chessboard.get_king_square<side>(), chessboard.get_occupancy() ^ bb(from) ^ capture) & chessboard.get_orthogonal_pieces<their_side>()) ) {
                moves.add(chess_move(from, ep_square, EN_PASSANT));
            }
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_pawn_pushes_moves(const board &chessboard, uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr std::uint64_t Rank3 = (side == White) ? ranks[RANK_3] : ranks[RANK_6];
    constexpr std::uint64_t Rank7 = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
    constexpr Direction Up = (side == White) ? NORTH : SOUTH;
    constexpr Direction UpLeft = (side == White) ?  NORTH_WEST : SOUTH_EAST;
    constexpr Direction UpRight = (side == White) ? NORTH_EAST : SOUTH_WEST;

    const std::uint64_t empty = ~chessboard.get_occupancy();
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pin = data.orthogonal_pins;
    const std::uint64_t diagonal_pin = data.diagonal_pins;
    const std::uint64_t checkmask = data.checkmask;

    // Single & Double Push
    if constexpr (!only_captures) {
        std::uint64_t pawns = source & ~Rank7 & ~diagonal_pin;
        std::uint64_t single_pushes = (shift<Up>(pawns & ~orthogonal_pin) | (shift<Up>(pawns & orthogonal_pin) & orthogonal_pin)) & empty;
        std::uint64_t double_pushes = shift<Up>(single_pushes & Rank3) & empty;

        if constexpr (in_check) {
            single_pushes &= checkmask;
            double_pushes &= checkmask;
        }

        while(single_pushes) {
            const Square to = pop_lsb(single_pushes);
            const Square from = to - Up;
            moves.add(chess_move(from, to, QUIET));
        }

        while(double_pushes) {
            const Square to = pop_lsb(double_pushes);
            const Square from = to - Up - Up;
            moves.add(chess_move(from, to, DOUBLE_PAWN_PUSH));
        }
    }

    // Captures
    std::uint64_t pawns = source & ~Rank7 & ~orthogonal_pin;
    std::uint64_t left_captures = (shift<UpLeft>(pawns & ~diagonal_pin) | (shift<UpLeft>(pawns & diagonal_pin) & diagonal_pin)) & enemy;
    std::uint64_t right_captures = (shift<UpRight>(pawns & ~diagonal_pin) | (shift<UpRight>(pawns & diagonal_pin) & diagonal_pin)) & enemy;

    if constexpr (in_check) {
        left_captures &= checkmask;
        right_captures &= checkmask;
    }

    while(left_captures) {
        const Square to = pop_lsb(left_captures);
        const Square from = to - UpLeft;
        moves.add(chess_move(from, to, CAPTURE));
    }

    while(right_captures) {
        const Square to = pop_lsb(right_captures);
        const Square from = to - UpRight;
        moves.add(chess_move(from, to, CAPTURE));
    }
}

template<Color side, bool InCheck, bool only_captures>
void generate_pawn_moves(const board &chessboard, std::uint64_t source, move_list & moves, generator_data & data) {
    generate_pawn_pushes_moves<side, InCheck, only_captures>(chessboard, source, moves, data);
    generate_promotions<side, InCheck, only_captures>(chessboard, source, moves, data);
    generate_enpassant<side, InCheck>(chessboard, source, moves, data);
}

template<Color side>
void generate_castling_moves(const board &chessboard, move_list &moves, generator_data & data) {
    constexpr CastlingRight kingside = side == White ? CASTLE_WHITE_KINGSIDE : CASTLE_BLACK_KINGSIDE;
    constexpr CastlingRight queenside = side == White ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;
    constexpr Square king_from = side == White ? E1 : E8;
    constexpr Square kingside_to = side == White ? G1 : G8;
    constexpr Square queenside_to = side == White ? C1 : C8;
    constexpr chess_move kingside_castle = chess_move(king_from, kingside_to, KING_CASTLE);
    constexpr chess_move queenside_castle = chess_move(king_from, queenside_to, QUEEN_CASTLE);
    constexpr std::uint64_t kingside_path = side == White ? 0x60ull : 0x6000000000000000ull;
    constexpr std::uint64_t queenside_path = side == White ? 0xeull : 0xe00000000000000ull;
    constexpr std::uint64_t queenside_king_path = side == White ? 0xcull : 0xc00000000000000ull;

    const std::uint64_t occupancy = chessboard.get_occupancy();
    const std::uint64_t attacked_squares = data.threats;

    if (chessboard.can_castle(kingside) && !((occupancy | attacked_squares) & kingside_path)) {
        moves.add(kingside_castle);
    }

    if (chessboard.can_castle(queenside) && !((occupancy & queenside_path) | attacked_squares & queenside_king_path)) {
        moves.add(queenside_castle);
    }
}

template<Color side, bool only_captures>
void generate_king_moves(const board &chessboard, Square from, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();
    std::uint64_t dest = KING_ATTACKS[from] & ~chessboard.get_side_occupancy<side>() & ~data.threats;

    if constexpr (!only_captures) {
        std::uint64_t quiets = dest & ~enemy;

        while (quiets) {
            const Square to = pop_lsb(quiets);
            moves.add(chess_move(from, to, QUIET));
        }
    }

    std::uint64_t captures = dest & enemy;

    while (captures) {
        const Square to = pop_lsb(captures);
        moves.add(chess_move(from, to, CAPTURE));
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_knight_moves(const board &chessboard, std::uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    std::uint64_t knights = source & ~(data.diagonal_pins | data.orthogonal_pins);
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();

    while (knights) {
        const Square from = pop_lsb(knights);
        std::uint64_t dest = KNIGHT_ATTACKS[from] & ~chessboard.get_side_occupancy<side>();

        if constexpr (in_check) dest &= data.checkmask;
        if constexpr (!only_captures) {
            std::uint64_t quiets = dest & ~enemy;

            while (quiets) {
                const Square to = pop_lsb(quiets);
                moves.add(chess_move(from, to, QUIET));
            }
        }

        std::uint64_t captures = dest & enemy;

        while (captures) {
            const Square to = pop_lsb(captures);
            moves.add(chess_move(from, to, CAPTURE));
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_bishop_moves(const board &chessboard, std::uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pins = data.orthogonal_pins;
    const std::uint64_t diagonal_pins = data.diagonal_pins;

    // non-pinned
    std::uint64_t pieces = source & ~orthogonal_pins & ~diagonal_pins;
    while(pieces) {
        const Square from = pop_lsb(pieces);
        std::uint64_t dest = attacks<Ray::BISHOP>(from, chessboard.get_occupancy()) & ~chessboard.get_side_occupancy<side>();

        if constexpr (in_check) dest &= data.checkmask;
        if constexpr (!only_captures) {
            std::uint64_t quiets = dest & ~enemy;

            while (quiets) {
                Square to = pop_lsb(quiets);
                moves.add(chess_move(from, to, QUIET));
            }
        }

        std::uint64_t captures = dest & enemy;

        while (captures) {
            Square to = pop_lsb(captures);
            moves.add(chess_move(from, to, CAPTURE));
        }
    }

    // pinned
    pieces = source & ~orthogonal_pins & diagonal_pins;
    while (pieces) {
        const Square from = pop_lsb(pieces);
        std::uint64_t dest = attacks<Ray::BISHOP>(from, chessboard.get_occupancy()) & ~chessboard.get_side_occupancy<side>() & diagonal_pins;

        if constexpr (in_check) dest &= data.checkmask;
        if constexpr (!only_captures) {
            std::uint64_t quiets = dest & ~enemy;

            while (quiets) {
                const Square to = pop_lsb(quiets);
                moves.add(chess_move(from, to, QUIET));
            }
        }

        std::uint64_t captures = dest & enemy;

        while (captures) {
            const Square to = pop_lsb(captures);
            moves.add(chess_move(from, to, CAPTURE));
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_rook_moves(const board &chessboard, std::uint64_t source, move_list &moves, generator_data & data) {
    constexpr Color their_side = side == White ? Black : White;
    const std::uint64_t enemy = chessboard.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pins = data.orthogonal_pins;
    const std::uint64_t diagonal_pins = data.diagonal_pins;

    // non-pinned
    std::uint64_t rooks = source & ~diagonal_pins & ~orthogonal_pins;
    while(rooks) {
        const Square from = pop_lsb(rooks);
        std::uint64_t dest = attacks<Ray::ROOK>(from, chessboard.get_occupancy()) & ~chessboard.get_side_occupancy<side>();

        if constexpr (in_check) dest &= data.checkmask;
        if constexpr (!only_captures) {
            std::uint64_t quiets = dest & ~enemy;

            while (quiets) {
                const Square to = pop_lsb(quiets);
                moves.add(chess_move(from, to, QUIET));
            }
        }

        std::uint64_t captures = dest & enemy;

        while (captures) {
            const Square to = pop_lsb(captures);
            moves.add(chess_move(from, to, CAPTURE));
        }
    }

    // pinned
    rooks = source & ~diagonal_pins & orthogonal_pins;
    while(rooks) {
        const Square from = pop_lsb(rooks);
        std::uint64_t dest = attacks<Ray::ROOK>(from, chessboard.get_occupancy()) & ~chessboard.get_side_occupancy<side>() & orthogonal_pins;

        if constexpr (in_check) dest &= data.checkmask;
        if constexpr (!only_captures) {
            std::uint64_t quiets = dest & ~enemy;

            while (quiets) {
                const Square to = pop_lsb(quiets);
                moves.add(chess_move(from, to, QUIET));
            }
        }

        std::uint64_t captures = dest & enemy;

        while (captures) {
            const Square to = pop_lsb(captures);
            moves.add(chess_move(from, to, CAPTURE));
        }
    }
}

template<Color side, bool only_captures = false>
void generate_all_moves(board &chessboard, move_list &moves) {
    chessboard.calculate_pins<side>();
    generator_data data = chessboard.get_generator_data();
    switch(popcount(data.checkers)) {
        case 0:
            generate_pawn_moves<side, false, only_captures>(chessboard, chessboard.get_pieces(side, Pawn), moves, data);
            generate_knight_moves<side, false, only_captures>(chessboard, chessboard.get_pieces(side, Knight), moves, data);
            generate_bishop_moves<side, false, only_captures>(chessboard, chessboard.get_diagonal_pieces<side>(), moves, data);
            generate_rook_moves<side, false, only_captures>(chessboard, chessboard.get_orthogonal_pieces<side>(), moves, data);
            if constexpr (!only_captures) generate_castling_moves<side>(chessboard, moves, data);
            generate_king_moves<side, only_captures>(chessboard, chessboard.get_king_square<side>(), moves, data);
            return;
        case 1:
            generate_pawn_moves<side, true, only_captures>(chessboard, chessboard.get_pieces(side, Pawn), moves, data);
            generate_knight_moves<side, true, only_captures>(chessboard, chessboard.get_pieces(side, Knight), moves, data);
            generate_bishop_moves<side, true, only_captures>(chessboard, chessboard.get_diagonal_pieces<side>(), moves, data);
            generate_rook_moves<side, true, only_captures>(chessboard, chessboard.get_orthogonal_pieces<side>(), moves, data);
            generate_king_moves<side, only_captures>(chessboard, chessboard.get_king_square<side>(), moves, data);
            return;
        default:
            generate_king_moves<side, only_captures>(chessboard, chessboard.get_king_square<side>(), moves, data);
            return;
    }
}

#endif //MOTOR_MOVE_GENERATOR_HPP