#ifndef MOTOR_MOVE_GENERATOR_HPP
#define MOTOR_MOVE_GENERATOR_HPP

#include "../chess_board/board.hpp"
#include "move_list.hpp"

template<Color side, bool in_check, bool only_captures>
void generate_promotions(const board & pos, std::uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr std::uint64_t penultimate_rank = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
    constexpr Direction up = (side == White) ? NORTH : SOUTH;
    constexpr Direction up_left = (side == White) ? NORTH_WEST : SOUTH_EAST;
    constexpr Direction up_right = (side == White) ? NORTH_EAST : SOUTH_WEST;

    const std::uint64_t empty = ~pos.get_occupancy();
    const std::uint64_t orthogonal_pins = pos.pin_orthogonal();
    const std::uint64_t diagonal_pins = pos.pin_diagonal();
    const std::uint64_t checkmask = pos.checkmask();

    std::uint64_t pawns_penultimate = source & penultimate_rank;
    if (pawns_penultimate) {
        // Capture Promotion
        std::uint64_t pawns = pawns_penultimate & ~orthogonal_pins;
        std::uint64_t left_promotions = (shift<up_left>(pawns & ~diagonal_pins) | (shift<up_left>(pawns & diagonal_pins) & diagonal_pins)) & pos.get_side_occupancy<their_side>();
        std::uint64_t right_promotions = (shift<up_right>(pawns & ~diagonal_pins) | (shift<up_right>(pawns & diagonal_pins) & diagonal_pins)) & pos.get_side_occupancy<their_side>();

        if constexpr (in_check) {
            left_promotions &= checkmask;
            right_promotions &= checkmask;
        }

        while(left_promotions) {
            Square to = pop_lsb(left_promotions);
            Square from = to - up_left;
            moves.push_back(chess_move(from, to, PROMOTION, QueenPromotion));

            if constexpr (!only_captures) {
                moves.push_back(chess_move(from, to, PROMOTION, KnightPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, RookPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, BishopPromotion));
            }
        }

        while(right_promotions) {
            const Square to = pop_lsb(right_promotions);
            const Square from = to - up_right;
            moves.push_back(chess_move(from, to, PROMOTION, QueenPromotion));

            if constexpr (!only_captures) {
                moves.push_back(chess_move(from, to, PROMOTION, KnightPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, RookPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, BishopPromotion));
            }
        }

        // Quiet Promotion
        pawns = pawns_penultimate & ~diagonal_pins;
        std::uint64_t quiet_promotions = (shift<up>(pawns & ~orthogonal_pins) | (shift<up>(pawns & orthogonal_pins) & orthogonal_pins)) & empty;

        if constexpr (in_check) quiet_promotions &= checkmask;

        while(quiet_promotions) {
            Square to = pop_lsb(quiet_promotions);
            Square from = to - up;
            moves.push_back(chess_move(from, to, PROMOTION, QueenPromotion));

            if constexpr (!only_captures) {
                moves.push_back(chess_move(from, to, PROMOTION, KnightPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, RookPromotion));
                moves.push_back(chess_move(from, to, PROMOTION, BishopPromotion));
            }
        }
    }
}

template<Color side, bool in_check>
void generate_enpassant(const board &pos, std::uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr Direction pawn_direction = side == White ? NORTH : SOUTH;

    Square ep_square = pos.enpassant_square();
    // Enpassant
    if (ep_square != Square::Null_Square) {
        std::uint64_t orthogonal_pins = pos.pin_orthogonal();
        std::uint64_t diagonal_pins = pos.pin_diagonal();
        std::uint64_t checkmask = pos.checkmask();

        std::uint64_t capture = bb(pos.enpassant_square() - pawn_direction); // Pawn who will be captured enpassant
        std::uint64_t enpassants = PAWN_ATTACKS_TABLE[their_side][pos.enpassant_square()] & source & ~orthogonal_pins;

        if constexpr (in_check) {
            if (!(pos.checkers() & capture)) {
                enpassants &= checkmask;
            }
        }

        while(enpassants) {
            Square from = pop_lsb(enpassants);

            if ((bb(from) & diagonal_pins) && !(bb(ep_square) & diagonal_pins))
                continue;

            if (!(attacks<Ray::HORIZONTAL>(pos.get_king_square<side>(), pos.get_occupancy() ^ bb(from) ^ capture) & pos.get_orthogonal_pieces<their_side>()) )
            {
                moves.push_back(chess_move(from, ep_square, EN_PASSANT));
            }
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_pawn_pushes_moves(const board &pos, uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr std::uint64_t Rank3 = (side == White) ? ranks[RANK_3] : ranks[RANK_6];
    constexpr std::uint64_t Rank7 = (side == White) ? ranks[RANK_7] : ranks[RANK_2];
    constexpr Direction Up = (side == White) ? NORTH : SOUTH;
    constexpr Direction UpLeft = (side == White) ?  NORTH_WEST : SOUTH_EAST;
    constexpr Direction UpRight = (side == White) ? NORTH_EAST : SOUTH_WEST;

    const std::uint64_t empty = ~pos.get_occupancy();
    const std::uint64_t orthogonal_pin = pos.pin_orthogonal();
    const std::uint64_t diagonal_pin = pos.pin_diagonal();
    const std::uint64_t checkmask = pos.checkmask();

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
            moves.push_back(chess_move(from, to, NORMAL));
        }

        while(double_pushes) {
            const Square to = pop_lsb(double_pushes);
            const Square from = to - Up - Up;
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }

    // Captures
    std::uint64_t pawns = source & ~Rank7 & ~orthogonal_pin;
    std::uint64_t left_captures = (shift<UpLeft>(pawns & ~diagonal_pin) | (shift<UpLeft>(pawns & diagonal_pin) & diagonal_pin)) & pos.get_side_occupancy<their_side>();
    std::uint64_t right_captures = (shift<UpRight>(pawns & ~diagonal_pin) | (shift<UpRight>(pawns & diagonal_pin) & diagonal_pin)) & pos.get_side_occupancy<their_side>();

    if constexpr (in_check) {
        left_captures &= checkmask;
        right_captures &= checkmask;
    }

    while(left_captures) {
        const Square to = pop_lsb(left_captures);
        const Square from = to - UpLeft;
        moves.push_back(chess_move(from, to, NORMAL));
    }

    while(right_captures) {
        const Square to = pop_lsb(right_captures);
        const Square from = to - UpRight;
        moves.push_back(chess_move(from, to, NORMAL));
    }
}

template<Color side, bool InCheck, bool only_captures>
void generate_pawn_moves(const board &pos, std::uint64_t source, move_list & moves) {
    generate_pawn_pushes_moves<side, InCheck, only_captures>(pos, source, moves);
    generate_promotions<side, InCheck, only_captures>(pos, source, moves);
    generate_enpassant<side, InCheck>(pos, source, moves);
}

template<Color side>
void generate_castling_moves(const board &pos, move_list &moves) {
    constexpr CastlingRight kingside = side == White ? CASTLE_WHITE_KINGSIDE : CASTLE_BLACK_KINGSIDE;
    constexpr CastlingRight queenside = side == White ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;
    constexpr Square king_from = side == White ? E1 : E8;
    constexpr Square kingside_to = side == White ? G1 : G8;
    constexpr Square queenside_to = side == White ? C1 : C8;
    constexpr chess_move kingside_castle = chess_move(king_from, kingside_to, CASTLING);
    constexpr chess_move queenside_castle = chess_move(king_from, queenside_to, CASTLING);

    const std::uint64_t occupancy = pos.get_occupancy();
    const std::uint64_t attacked_squares = pos.checked_squares();

    if (pos.can_castle(kingside) && !((occupancy | attacked_squares) & CastlingPath[kingside])) {
        moves.push_back(kingside_castle);
    }

    if (pos.can_castle(queenside) && !((occupancy & CastlingPath[queenside]) | (attacked_squares & CastlingKingPath[queenside]))) {
        moves.push_back(queenside_castle);
    }
}

template<Color side, bool only_captures>
void generate_king_moves(const board &pos, Square from, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    std::uint64_t dest = KING_ATTACKS[from] & ~pos.get_side_occupancy<side>() & ~pos.checked_squares();

    if constexpr (only_captures) dest &= pos.get_side_occupancy<their_side>();

    while(dest) {
        const Square to = pop_lsb(dest);
        moves.push_back(chess_move(from, to, NORMAL));
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_knight_moves(const board &pos, std::uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    std::uint64_t knights = source & ~(pos.pin_diagonal() | pos.pin_orthogonal());

    while (knights) {
        const Square from = pop_lsb(knights);
        std::uint64_t dest = KNIGHT_ATTACKS[from] & ~pos.get_side_occupancy<side>();

        if constexpr (in_check) dest &= pos.checkmask();
        if constexpr (only_captures) dest &= pos.get_side_occupancy<their_side>();

        while (dest) {
            const Square to = pop_lsb(dest);
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_bishop_moves(const board &pos, std::uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    const std::uint64_t enemy = pos.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pins = pos.pin_orthogonal();
    const std::uint64_t diagonal_pins = pos.pin_diagonal();

    // non-pinned
    std::uint64_t pieces = source & ~orthogonal_pins & ~diagonal_pins;
    while(pieces) {
        const Square from = pop_lsb(pieces);
        std::uint64_t dest = attacks<Ray::BISHOP>(from, pos.get_occupancy()) & ~pos.get_side_occupancy<side>();

        if constexpr (in_check) dest &= pos.checkmask();
        if constexpr (only_captures) dest &= enemy;

        while(dest) {
            const Square to = pop_lsb(dest);
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }

    // pinned
    pieces = source & ~orthogonal_pins & diagonal_pins;
    while (pieces) {
        const Square from = pop_lsb(pieces);
        std::uint64_t dest = attacks<Ray::BISHOP>(from, pos.get_occupancy()) & ~pos.get_side_occupancy<side>() & diagonal_pins;

        if constexpr (in_check) dest &= pos.checkmask();
        if constexpr (only_captures) dest &= enemy;

        while(dest) {
            const Square to = pop_lsb(dest);
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }
}

template<Color side, bool in_check, bool only_captures>
void generate_rook_moves(const board &pos, std::uint64_t source, move_list &moves) {
    constexpr Color their_side = side == White ? Black : White;
    const std::uint64_t enemy = pos.get_side_occupancy<their_side>();
    const std::uint64_t orthogonal_pins = pos.pin_orthogonal();
    const std::uint64_t diagonal_pins = pos.pin_diagonal();

    // non-pinned
    std::uint64_t rooks = source & ~diagonal_pins & ~orthogonal_pins;
    while(rooks) {
        const Square from = pop_lsb(rooks);
        std::uint64_t dest = attacks<Ray::ROOK>(from, pos.get_occupancy()) & ~pos.get_side_occupancy<side>();

        if constexpr (in_check) dest &= pos.checkmask();
        if constexpr (only_captures) dest &= enemy;

        while(dest) {
            Square to = pop_lsb(dest);
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }

    // pinned
    rooks = source & ~diagonal_pins & orthogonal_pins;
    while(rooks) {
        Square from = pop_lsb(rooks);
        std::uint64_t dest = attacks<Ray::ROOK>(from, pos.get_occupancy()) & ~pos.get_side_occupancy<side>() & orthogonal_pins;

        if constexpr (in_check) dest &= pos.checkmask();
        if constexpr (only_captures) dest &= enemy;

        while(dest) {
            Square to = pop_lsb(dest);
            moves.push_back(chess_move(from, to, NORMAL));
        }
    }
}

template<Color side, bool only_captures = false>
void generate_all_moves(board &pos, move_list &moves) {
    pos.calculate_pins<side>();
    switch(popcount(pos.checkers())) {
        case 0:
            generate_pawn_moves<side, false, only_captures>(pos, pos.get_pieces(side, Pawn), moves);
            generate_knight_moves<side, false, only_captures>(pos, pos.get_pieces(side, Knight), moves);
            generate_bishop_moves<side, false, only_captures>(pos, pos.get_diagonal_pieces<side>(), moves);
            generate_rook_moves<side, false, only_captures>(pos, pos.get_orthogonal_pieces<side>(), moves);
            if constexpr (!only_captures) generate_castling_moves<side>(pos, moves);
            generate_king_moves<side, only_captures>(pos, pos.get_king_square<side>(), moves);
            return;
        case 1:
            generate_pawn_moves<side, true, only_captures>(pos, pos.get_pieces(side, Pawn), moves);
            generate_knight_moves<side, true, only_captures>(pos, pos.get_pieces(side, Knight), moves);
            generate_bishop_moves<side, true, only_captures>(pos, pos.get_diagonal_pieces<side>(), moves);
            generate_rook_moves<side, true, only_captures>(pos, pos.get_orthogonal_pieces<side>(), moves);
            generate_king_moves<side, only_captures>(pos, pos.get_king_square<side>(), moves);
            return;
        default:
            generate_king_moves<side, only_captures>(pos, pos.get_king_square<side>(), moves);
            return;
    }
}

#endif //MOTOR_MOVE_GENERATOR_HPP