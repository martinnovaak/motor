#ifndef MOTOR_MOVEGEN_H
#define MOTOR_MOVEGEN_H

#include "board.h"
#include "pinmask.h"
#include "movelist.h"

template <Color our_color>
void generate_pawn_moves(board & b, movelist & ml, uint64_t checkmask, uint64_t move_h, uint64_t move_v, uint64_t move_a, uint64_t move_d, uint64_t enemy, uint64_t empty) {
    constexpr Color their_color = (our_color == WHITE) ? BLACK : WHITE;
    constexpr uint64_t penultimate_rank = (our_color == WHITE ? ranks[RANK_7] : ranks[RANK_2]);
    constexpr uint64_t not_penultimate_rank = (our_color == WHITE) ? not_penultimate_ranks[WHITE] : not_penultimate_ranks[BLACK];
    constexpr uint64_t enpassant_rank = (our_color == WHITE ? ranks[RANK_3] : ranks[RANK_6]);
    constexpr Direction up = (our_color == WHITE ? NORTH : SOUTH);
    constexpr Direction up_2 = (our_color == WHITE ? NORTH_2 : SOUTH_2);
    constexpr Direction antidiagonal_capture = (our_color == WHITE ? NORTH_WEST : SOUTH_EAST);
    constexpr Direction diagonal_capture = (our_color == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction antidiagonal_enpassant = (our_color == BLACK ? NORTH_WEST : SOUTH_EAST);
    constexpr Direction diagonal_enpassant = (our_color == BLACK ? NORTH_EAST : SOUTH_WEST);

    const uint64_t pawns = b.get_pawn_bitboard() & move_h; // horizontally pinned pawns cannot move

    const uint64_t pawns_penultimate = pawns & penultimate_rank;
    const uint64_t pawns_not_penultimate = pawns & not_penultimate_rank;

    const uint64_t blocking_squares = empty & checkmask;
    const uint64_t threat_squares = enemy & checkmask;

    // PAWN PUSH

    uint64_t bitboard_push_1 = shift<up>(pawns_not_penultimate & move_v) & empty;
    uint64_t bitboard_push_2 = shift<up>(bitboard_push_1 & enpassant_rank) & blocking_squares;

    bitboard_push_1 &= checkmask;

    while(bitboard_push_1) {
        int pawn_square = pop_lsb(bitboard_push_1);
        ml.add(move_t(pawn_square - up, pawn_square, our_color, PAWN));
    }

    while(bitboard_push_2) {
        int pawn_square = pop_lsb(bitboard_push_2);
        ml.add(move_t(pawn_square - up_2, pawn_square, our_color, PAWN, DOUBLE_PAWN_PUSH));
    }

    // PAWNS PROMOTE

    if (pawns_penultimate) {
        uint64_t bitboard_promote_push = shift<up>(pawns_penultimate & move_v) & blocking_squares;
        uint64_t bitboard_promote_antidiagonal = shift<antidiagonal_capture>(pawns_penultimate & move_a) & threat_squares;
        uint64_t bitboard_promote_diagonal = shift<diagonal_capture>(pawns_penultimate & move_d) & threat_squares;

        while (bitboard_promote_push) {
            int pawn_square = pop_lsb(bitboard_promote_push);
            ml.add(move_t(pawn_square - up, pawn_square, our_color, PAWN, QUEEN_PROMOTION));
            ml.add(move_t(pawn_square - up, pawn_square, our_color, PAWN, ROOK_PROMOTION));
            ml.add(move_t(pawn_square - up, pawn_square, our_color, PAWN, BISHOP_PROMOTION));
            ml.add(move_t(pawn_square - up, pawn_square, our_color, PAWN, KNIGHT_PROMOTION));
        }

        while (bitboard_promote_antidiagonal) {
            int pawn_square = pop_lsb(bitboard_promote_antidiagonal);
            PieceType attacked_piece = b.get_piece(pawn_square);
            ml.add(move_t(pawn_square - antidiagonal_capture, pawn_square, our_color, PAWN, QUEEN_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - antidiagonal_capture, pawn_square, our_color, PAWN, ROOK_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - antidiagonal_capture, pawn_square, our_color, PAWN, BISHOP_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - antidiagonal_capture, pawn_square, our_color, PAWN, KNIGHT_PROMOTION_CAPTURE, attacked_piece));
        }

        while (bitboard_promote_diagonal) {
            int pawn_square = pop_lsb(bitboard_promote_diagonal);
            PieceType attacked_piece = b.get_piece(pawn_square);
            ml.add(move_t(pawn_square - diagonal_capture, pawn_square, our_color, PAWN, QUEEN_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - diagonal_capture, pawn_square, our_color, PAWN, ROOK_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - diagonal_capture, pawn_square, our_color, PAWN, BISHOP_PROMOTION_CAPTURE, attacked_piece));
            ml.add(move_t(pawn_square - diagonal_capture, pawn_square, our_color, PAWN, KNIGHT_PROMOTION_CAPTURE, attacked_piece));
        }
    }

    // PAWN CAPTURE

    uint64_t bitboard_left_capture = shift<antidiagonal_capture>(pawns_not_penultimate & move_a) & threat_squares;
    uint64_t bitboard_right_capture = shift<diagonal_capture>(pawns_not_penultimate & move_d) & threat_squares;

    while(bitboard_left_capture) {
        int pawn_square = pop_lsb(bitboard_left_capture);
        ml.add(move_t(pawn_square - antidiagonal_capture, pawn_square, our_color, PAWN, CAPTURE, b.get_piece(pawn_square)));
    }

    while(bitboard_right_capture) {
        int pawn_square = pop_lsb(bitboard_right_capture);
        ml.add(move_t(pawn_square - diagonal_capture, pawn_square, our_color, PAWN, CAPTURE, b.get_piece(pawn_square)));
    }

    // en passant

    int enpassant_square = b.enpassant_square();
    if (enpassant_square != N_SQUARES)
    {
        uint64_t enpassant_bitboard = (1ull << enpassant_square);
        uint64_t enpassant_antidiagonal = shift<antidiagonal_capture>(pawns_not_penultimate & move_a) & enpassant_bitboard;
        if(enpassant_antidiagonal && b.check_legality_of_enpassant<their_color>(enpassant_square - antidiagonal_capture, enpassant_square - up)) {
            ml.add(move_t(enpassant_square - antidiagonal_capture, enpassant_square, our_color, PAWN, EN_PASSANT, PAWN));
        }
        uint64_t enpassant_diagonal = shift<diagonal_capture>(pawns_not_penultimate & move_d) & enpassant_bitboard;
        if(enpassant_diagonal && b.check_legality_of_enpassant<their_color>(enpassant_square - diagonal_capture, enpassant_square - up)) {
            ml.add(move_t(enpassant_square - diagonal_capture, enpassant_square, our_color, PAWN, EN_PASSANT, PAWN));
        }


        /*
        auto b1 = pawns_not_penultimate & PAWN_ATTACKS_TABLE[their_color][enpassant_square];

        while (b1) {
            int square_from = pop_lsb(b1);
            if (b.check_legality_of_enpassant<their_color>(square_from, enpassant_square - up)) {
                ml.add(move_t(square_from, enpassant_square, our_color, PAWN, EN_PASSANT, PAWN));
            }
        }
        */
    }
}

template <Color our_color, PieceType pt, bool pinHV, bool pinDA>
void generate_slider_moves(board & b, movelist & ml, uint64_t piece_bitboard, uint64_t checkmask, uint64_t enemy, uint64_t empty, uint64_t occupancy)
{
    while(piece_bitboard) {
        int square = pop_lsb(piece_bitboard);
        uint64_t attack_bitboard = attacks<pt>(square, occupancy) & checkmask;

        if constexpr (pinHV) {
            attack_bitboard &= rook_mask[square];
        }

        if constexpr (pinDA) {
            attack_bitboard &= bishop_mask[square];
        }

        uint64_t captures = attack_bitboard & enemy;
        uint64_t quiets = attack_bitboard & empty;

        while(captures) {
            int target_square = pop_lsb(captures);
            ml.add(move_t(square, target_square, our_color, pt, CAPTURE, b.get_piece(target_square)));
        }

        while(quiets) {
            int target_square = pop_lsb(quiets);
            ml.add(move_t(square, target_square, our_color, pt));
        }
    }
}

template <Color our_color>
void generate_knight_moves(board & b, movelist & ml, uint64_t target, uint64_t pin, uint64_t enemy, uint64_t empty, uint64_t occupancy)
{
    uint64_t piece_bitboard = b.get_knight_bitboard() & pin; // pinned knight cannot move_t
    while(piece_bitboard) {
        int square = pop_lsb(piece_bitboard);
        uint64_t attack_bitboard = attacks<KNIGHT>(square, occupancy) & target;
        uint64_t captures = attack_bitboard & enemy;
        while(captures) {
            int target_square = pop_lsb(captures);
            ml.add(move_t(square, target_square, our_color, KNIGHT, CAPTURE, b.get_piece(target_square)));
        }
        uint64_t quiets = attack_bitboard & empty;
        while(quiets) {
            int target_square = pop_lsb(quiets);
            ml.add(move_t(square, target_square, our_color, KNIGHT));
        }
    }
}

template <Color our_color>
void generate_king_moves(board &b, movelist & ml, int king_square, uint64_t safe_squares, uint64_t empty, uint64_t enemy)
{
    uint64_t king_moves = KING_ATTACKS[king_square] & safe_squares;

    uint64_t king_quiets = king_moves & empty;
    while(king_quiets) {
        int target_square = pop_lsb(king_quiets);
        ml.add(move_t(king_square, target_square, our_color, KING));
    }

    uint64_t king_captures = king_moves & enemy;
    while (king_captures) {
        int target_square = pop_lsb(king_captures);
        ml.add(move_t(king_square, target_square, our_color, KING, CAPTURE, b.get_piece(target_square)));
    }
}

template <Color our_color>
void generate_castle_moves(movelist & ml, int castling_right, uint64_t safe_squares, uint64_t empty) {
    constexpr int king_e_square = our_color == WHITE ? E1 : E8;
    constexpr int king_g_square = our_color == WHITE ? G1 : G8;
    constexpr int king_c_square = our_color == WHITE ? C1 : C8;
    constexpr uint64_t kingside_castle_efg_mask = our_color == WHITE ? 0x70ull : 0x7000000000000000ull;
    constexpr uint64_t kingside_castle_fg_mask = our_color == WHITE ? 0x60ull : 0x6000000000000000ull;
    constexpr uint64_t queenside_castle_cde_mask = our_color == WHITE ? 0x1cull : 0x1c00000000000000ull;
    constexpr uint64_t queenside_castle_bcd_mask = our_color == WHITE ? 0xeull : 0xe00000000000000ull;
    constexpr int kingside_castle_right = our_color == WHITE ? CASTLE_WHITE_KINGSIDE : CASTLE_BLACK_KINGSIDE;
    constexpr int queenside_castle_right = our_color == WHITE ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;

    if(castling_right & kingside_castle_right
    && (safe_squares & kingside_castle_efg_mask) == kingside_castle_efg_mask
    && (empty & kingside_castle_fg_mask) == kingside_castle_fg_mask)
    {
        ml.add(move_t(king_e_square, king_g_square, our_color, KING, KING_CASTLE));
    }

    if(castling_right & queenside_castle_right
       && (safe_squares & queenside_castle_cde_mask) == queenside_castle_cde_mask
       && (empty & queenside_castle_bcd_mask) == queenside_castle_bcd_mask)
    {
        ml.add(move_t(king_e_square, king_c_square, our_color, KING, QUEEN_CASTLE));
    }
}

template <Color our_color>
void generate_all_moves(board & b, movelist & ml) {
    constexpr Color enemy_color = our_color == WHITE ? BLACK : WHITE;

    int king_square = b.get_king_square();
    uint64_t enemy_pieces = b.get_enemy_bitboard<enemy_color>();
    uint64_t occupancy = b.get_occupancy();
    uint64_t empty = ~occupancy;
    uint64_t checkers = b.get_checkers<enemy_color>();

    uint64_t safe_squares = b.get_safe_squares<enemy_color>(king_square);

    uint64_t checkmask = b.get_checkmask(checkers, king_square);

    generate_king_moves<our_color>(b, ml, king_square, safe_squares, empty, enemy_pieces);

    if(checkmask == 0)
        return;

    auto [pin_h, pin_v, pin_a, pin_d] = b.get_pinners<our_color, enemy_color>(king_square);
    auto [rook_bitboard, bishop_bitboard, queen_bitboard] = b.get_slider_bitboards<our_color>();
    uint64_t pin_hv = pin_h | pin_v;
    uint64_t pin_ad = pin_a | pin_d;
    rook_bitboard &= ~pin_ad; // rooks pinned diagonally cannot move_t
    bishop_bitboard &= ~pin_hv; // bishops pinned horizontally and vertically cannot move_t
    uint64_t non_pinned_pieces = ~(pin_ad | pin_hv);

    uint64_t hv_checkmask = checkmask & pin_hv;
    uint64_t ad_checkmask = checkmask & pin_ad;

    // non-pinned rooks
    generate_slider_moves<our_color, ROOK, false, false>(b, ml, rook_bitboard & ~pin_hv, checkmask, enemy_pieces, empty, occupancy);
    // horizontally/vertically pinned rooks
    generate_slider_moves<our_color, ROOK, true, false>(b, ml, rook_bitboard & pin_hv, hv_checkmask, enemy_pieces, empty, occupancy);
    // non-pinned bishops
    generate_slider_moves<our_color, BISHOP, false, false>(b, ml, bishop_bitboard & ~pin_ad, checkmask, enemy_pieces, empty, occupancy);
    // antidiagonally/diagonally pinned bishops
    generate_slider_moves<our_color, BISHOP, false, true>(b, ml, bishop_bitboard & pin_ad, ad_checkmask, enemy_pieces, empty, occupancy);
    // non-pinned queens
    generate_slider_moves<our_color, QUEEN, false, false>(b, ml, queen_bitboard & non_pinned_pieces, checkmask, enemy_pieces, empty, occupancy);
    // horizontally/vertically pinned queens
    generate_slider_moves<our_color, QUEEN, true, false>(b, ml, pin_hv & queen_bitboard, hv_checkmask, enemy_pieces, empty, occupancy);
    // antidiagonally/diagonally pinned queens
    generate_slider_moves<our_color, QUEEN, false, true>(b, ml, pin_ad & queen_bitboard, ad_checkmask, enemy_pieces, empty, occupancy);

    generate_knight_moves<our_color>(b, ml,checkmask, non_pinned_pieces, enemy_pieces, empty, occupancy);
    generate_pawn_moves<our_color>(b, ml, checkmask, ~pin_h, ~(pin_a | pin_d), ~(pin_d | pin_v), ~(pin_a | pin_v), enemy_pieces, empty);
    generate_castle_moves<our_color>(ml, b.get_castle_rights(),  safe_squares, empty);
}

void generate_moves(board & b, movelist & ml) {
    if(b.get_side() == WHITE) {
        generate_all_moves<WHITE>(b, ml);
    } else {
        generate_all_moves<BLACK>(b, ml);
    }
}

#endif //MOTOR_MOVEGEN_H
