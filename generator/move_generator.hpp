#ifndef MOTOR_MOVE_GENERATOR_HPP
#define MOTOR_MOVE_GENERATOR_HPP

#include "movelist.hpp"
#include "../chess/board.hpp"

template<Direction direction>
constexpr std::uint64_t shift(std::uint64_t bitboard) {
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

template <Color our_color, bool captures_only>
void generate_pawn_moves(const board & b, movelist & ml, std::uint64_t pawn_bitboard, std::uint64_t checkmask, std::uint64_t enemy, std::uint64_t empty) {
    constexpr Color their_color = (our_color == White) ? Black : White;
    constexpr std::uint64_t penultimate_rank = (our_color == White ? ranks[RANK_7] : ranks[RANK_2]);
    constexpr std::uint64_t not_penultimate_rank = (our_color == White) ? 0xFF00FFFFFFFFFFFFull : 0xFFFFFFFFFFFF00FFull;
    constexpr std::uint64_t enpassant_rank = (our_color == White ? ranks[RANK_3] : ranks[RANK_6]);
    constexpr Direction up   = (our_color == White ? NORTH   : SOUTH);
    constexpr Direction up_2 = (our_color == White ? NORTH_2 : SOUTH_2);
    constexpr Direction antidiagonal_capture   = (our_color == White ? NORTH_WEST : SOUTH_EAST);
    constexpr Direction diagonal_capture       = (our_color == White ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction antidiagonal_enpassant = (our_color == Black ? NORTH_WEST : SOUTH_EAST);
    constexpr Direction diagonal_enpassant     = (our_color == Black ? NORTH_EAST : SOUTH_WEST);

    const std::uint64_t pawns_penultimate     = pawn_bitboard & penultimate_rank;
    const std::uint64_t pawns_not_penultimate = pawn_bitboard & not_penultimate_rank;

    const std::uint64_t blocking_squares = empty & checkmask;
    const std::uint64_t capture_bitboard = enemy & checkmask;

    // PAWN PUSH
    if constexpr (!captures_only) {
        std::uint64_t bitboard_push_1 = shift<up>(pawns_not_penultimate) & empty;
        std::uint64_t bitboard_push_2 = shift<up>(bitboard_push_1 & enpassant_rank) & blocking_squares;

        bitboard_push_1 &= checkmask;

        while (bitboard_push_1) {
            Square pawn_square = pop_lsb(bitboard_push_1);
            ml.add(chessmove(pawn_square - up, pawn_square, MoveType::QUIET, Pawn));
        }

        while (bitboard_push_2) {
            Square pawn_square = pop_lsb(bitboard_push_2);
            ml.add(chessmove(pawn_square - up_2, pawn_square, MoveType::DOUBLE_PAWN_PUSH, Pawn));
        }
    }

    // PAWNS PROMOTE
    if (pawns_penultimate)
    {
        std::uint64_t bitboard_promote_push = shift<up>(pawns_penultimate) & blocking_squares;
        std::uint64_t bitboard_promote_antidiagonal = shift<antidiagonal_capture>(pawns_penultimate) & capture_bitboard;
        std::uint64_t bitboard_promote_diagonal = shift<diagonal_capture>(pawns_penultimate) & capture_bitboard;

        while (bitboard_promote_push) {
            Square pawn_square = pop_lsb(bitboard_promote_push);
            ml.add(chessmove(static_cast<Square>(pawn_square - up), pawn_square, QUEEN_PROMOTION, Pawn));
            if constexpr (!captures_only) {
                ml.add(chessmove(pawn_square - up, pawn_square, ROOK_PROMOTION, Pawn));
                ml.add(chessmove(pawn_square - up, pawn_square, BISHOP_PROMOTION, Pawn));
                ml.add(chessmove(pawn_square - up, pawn_square, KNIGHT_PROMOTION, Pawn));
            }
        }

        while (bitboard_promote_antidiagonal) {
            Square pawn_square = pop_lsb(bitboard_promote_antidiagonal);
            Piece captured_piece = b.get_piece(pawn_square);
            ml.add(chessmove(pawn_square - antidiagonal_capture, pawn_square, QUEEN_PROMOTION_CAPTURE, Pawn, captured_piece));
            if constexpr (!captures_only) {
                ml.add(chessmove(pawn_square - antidiagonal_capture, pawn_square, ROOK_PROMOTION_CAPTURE, Pawn, captured_piece));
                ml.add(chessmove(pawn_square - antidiagonal_capture, pawn_square, BISHOP_PROMOTION_CAPTURE, Pawn, captured_piece));
                ml.add(chessmove(pawn_square - antidiagonal_capture, pawn_square, KNIGHT_PROMOTION_CAPTURE, Pawn, captured_piece));
            }
        }

        while (bitboard_promote_diagonal) {
            Square pawn_square = pop_lsb(bitboard_promote_diagonal);
            Piece captured_piece = b.get_piece(pawn_square);
            ml.add(chessmove(pawn_square - diagonal_capture, pawn_square, QUEEN_PROMOTION_CAPTURE, Pawn, captured_piece));
            if constexpr (!captures_only) {
                ml.add(chessmove(pawn_square - diagonal_capture, pawn_square, ROOK_PROMOTION_CAPTURE, Pawn, captured_piece));
                ml.add(chessmove(pawn_square - diagonal_capture, pawn_square, BISHOP_PROMOTION_CAPTURE, Pawn, captured_piece));
                ml.add(chessmove(pawn_square - diagonal_capture, pawn_square, KNIGHT_PROMOTION_CAPTURE, Pawn, captured_piece));
            }
        }
    }

    // PAWN CAPTURES
    std::uint64_t bitboard_left_capture = shift<antidiagonal_capture>(pawns_not_penultimate) & capture_bitboard;
    std::uint64_t bitboard_right_capture = shift<diagonal_capture>(pawns_not_penultimate) & capture_bitboard;

    while(bitboard_left_capture) {
        Square pawn_square = pop_lsb(bitboard_left_capture);
        Piece captured_piece = b.get_piece(pawn_square);
        ml.add(chessmove(pawn_square - antidiagonal_capture, pawn_square, CAPTURE, Pawn, captured_piece));
    }

    while(bitboard_right_capture) {
        Square pawn_square = pop_lsb(bitboard_right_capture);
        Piece captured_piece = b.get_piece(pawn_square);
        ml.add(chessmove(pawn_square - diagonal_capture, pawn_square, CAPTURE, Pawn, captured_piece));
    }

    // EN PASSANT
    Square enpassant_square = b.enpassant_square();
    if (enpassant_square != Square::Null_Square)
    {
        std::uint64_t enpassant_bitboard = (1ull << enpassant_square);
        std::uint64_t enpassant_antidiagonal = shift<antidiagonal_capture>(pawns_not_penultimate) & enpassant_bitboard;
        if(enpassant_antidiagonal) {
            ml.add(chessmove(enpassant_square - antidiagonal_capture, enpassant_square, EN_PASSANT, Pawn, Pawn));
        }
        std::uint64_t enpassant_diagonal = shift<diagonal_capture>(pawns_not_penultimate) & enpassant_bitboard;
        if(enpassant_diagonal) {
            ml.add(chessmove(enpassant_square - diagonal_capture, enpassant_square, EN_PASSANT, Pawn, Pawn));
        }
    }
}

template <Color our_color, bool captures_only>
void generate_knight_moves(const board & b, movelist & ml, std::uint64_t knight_bitboard, std::uint64_t target, std::uint64_t enemy, std::uint64_t empty, std::uint64_t occupancy)
{
    while(knight_bitboard) {
        Square square = pop_lsb(knight_bitboard);
        std::uint64_t attack_bitboard = KNIGHT_ATTACKS[square] & target;

        std::uint64_t captures = attack_bitboard & enemy;
        while(captures) {
            Square target_square = pop_lsb(captures);
            Piece captured_piece = b.get_piece(target_square);
            ml.add(chessmove(square, target_square, CAPTURE, Knight, captured_piece));
        }

        if constexpr (!captures_only) {
            std::uint64_t quiets = attack_bitboard & empty;
            while (quiets) {
                Square target_square = pop_lsb(quiets);
                ml.add(chessmove(square, target_square, QUIET, Knight));
            }
        }
    }
}

template <Color our_color, bool captures_only>
void generate_king_moves(const board & b, movelist & ml, Square king_square, std::uint64_t safe_squares, std::uint64_t empty, std::uint64_t enemy)
{
    std::uint64_t king_moves = KING_ATTACKS[king_square] & safe_squares;

    std::uint64_t king_captures = king_moves & enemy;
    while (king_captures) {
        Square target_square = pop_lsb(king_captures);
        Piece captured_piece = b.get_piece(target_square);
        ml.add(chessmove(king_square, target_square, CAPTURE, King, captured_piece));
    }

    if constexpr (!captures_only) {
        std::uint64_t king_quiets = king_moves & empty;
        while (king_quiets) {
            Square target_square = pop_lsb(king_quiets);
            ml.add(chessmove(king_square, target_square, QUIET, King));
        }
    }
}

template <Color our_color>
void generate_castle_moves(movelist & ml, int castling_right, std::uint64_t safe_squares, std::uint64_t empty) {
    constexpr Square king_e_square = our_color == White ? E1 : E8;
    constexpr Square king_g_square = our_color == White ? G1 : G8;
    constexpr Square king_c_square = our_color == White ? C1 : C8;

    constexpr std::uint64_t kingside_castle_efg_mask  = our_color == White ? 0x70ull : 0x7000000000000000ull;
    constexpr std::uint64_t kingside_castle_fg_mask   = our_color == White ? 0x60ull : 0x6000000000000000ull;
    constexpr std::uint64_t queenside_castle_cde_mask = our_color == White ? 0x1cull : 0x1c00000000000000ull;
    constexpr std::uint64_t queenside_castle_bcd_mask = our_color == White ? 0xeull  : 0xe00000000000000ull;

    constexpr int kingside_castle_right  = our_color == White ? CASTLE_WHITE_KINGSIDE  : CASTLE_BLACK_KINGSIDE;
    constexpr int queenside_castle_right = our_color == White ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;

    if(castling_right & kingside_castle_right
       && (safe_squares & kingside_castle_efg_mask) == kingside_castle_efg_mask
       && (empty & kingside_castle_fg_mask) == kingside_castle_fg_mask)
    {
        ml.add(chessmove(king_e_square, king_g_square, KING_CASTLE, King));
    }

    if(castling_right & queenside_castle_right
       && (safe_squares & queenside_castle_cde_mask) == queenside_castle_cde_mask
       && (empty & queenside_castle_bcd_mask) == queenside_castle_bcd_mask)
    {
        ml.add(chessmove(king_e_square, king_c_square, QUEEN_CASTLE, King));
    }
}

template <Color our_color, Ray ray, Piece piece, bool captures_only>
void generate_slider_moves(const board & b, movelist& ml, std::uint64_t piece_bitboard, std::uint64_t checkmask, std::uint64_t enemy, std::uint64_t empty, std::uint64_t occupancy)
{
    while (piece_bitboard) {
        Square square = pop_lsb(piece_bitboard);
        std::uint64_t attack_bitboard = attacks<ray>(square, occupancy) & checkmask;

        std::uint64_t captures = attack_bitboard & enemy;

        while (captures) {
            Square target_square = pop_lsb(captures);
            Piece captured_piece = b.get_piece(target_square);
            ml.add(chessmove(square, target_square, CAPTURE, piece, captured_piece));
        }

        if constexpr (!captures_only) {
            std::uint64_t quiets = attack_bitboard & empty;
            while (quiets) {
                Square target_square = pop_lsb(quiets);
                ml.add(chessmove(square, target_square, QUIET, piece));
            }
        }
    }
}


template <Color our_color, bool captures_only>
bool generate_all_moves(board & b, movelist & ml) {
    constexpr Color enemy_color = our_color == White ? Black : White;

    Square king_square = b.get_king_square();
    const std::uint64_t enemy_pieces = b.get_side_occupancy<enemy_color>();
    const std::uint64_t occupancy = b.get_occupancy();
    const std::uint64_t empty = ~occupancy;

    const std::uint64_t safe_squares = b.get_safe_squares<enemy_color>(king_square);

    const std::uint64_t checkmask = b.get_checkmask<enemy_color>(king_square);

    generate_king_moves<our_color, captures_only>(b, ml, king_square, safe_squares, empty, enemy_pieces);

    std::uint64_t rook_bitboard   = b.get_pieces(our_color, Rook);
    std::uint64_t bishop_bitboard = b.get_pieces(our_color, Bishop);
    std::uint64_t queen_bitboard  = b.get_pieces(our_color, Queen);
    std::uint64_t knight_bitboard = b.get_pieces(our_color, Knight);
    std::uint64_t pawn_bitboard   = b.get_pieces(our_color, Pawn);

    // non-pinned rooks
    generate_slider_moves<our_color, Ray::ROOK, Rook, captures_only>(b, ml, rook_bitboard, checkmask,enemy_pieces, empty, occupancy);
     // non-pinned bishops
    generate_slider_moves<our_color, Ray::BISHOP, Bishop, captures_only>(b, ml, bishop_bitboard, checkmask,enemy_pieces, empty, occupancy);
    // non-pinned queens
    generate_slider_moves<our_color, Ray::QUEEN, Queen, captures_only>(b, ml, queen_bitboard, checkmask, enemy_pieces, empty, occupancy);
    generate_knight_moves<our_color, captures_only>(b, ml, knight_bitboard, checkmask, enemy_pieces, empty, occupancy);
    generate_pawn_moves<our_color, captures_only>(b, ml, pawn_bitboard, checkmask, enemy_pieces, empty);
    if constexpr (!captures_only) {
        generate_castle_moves<our_color>(ml, b.get_castle_rights(), safe_squares, empty);
    }

    return checkmask == full_board;
}

#endif //MOTOR_MOVE_GENERATOR_HPP