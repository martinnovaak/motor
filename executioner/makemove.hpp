#ifndef MOTOR_MAKEMOVE_HPP
#define MOTOR_MAKEMOVE_HPP

#include "../chess_board/board.hpp"
#include "../evaluation/nnue.hpp"

template <Color color>
std::int16_t evaluate(board& chessboard) {
    /*
    int game_phase =
    //(popcount(chessboard.get_pieces(White, Pawn) + chessboard.get_pieces(Black, Pawn))) * 6 +
    (popcount(chessboard.get_pieces(White, Knight) + chessboard.get_pieces(Black, Knight))) * 7 +
    (popcount(chessboard.get_pieces(White, Bishop) + chessboard.get_pieces(Black, Bishop))) * 8 +
    (popcount(chessboard.get_pieces(White, Rook) + chessboard.get_pieces(Black, Rook))) * 13 +
    (popcount(chessboard.get_pieces(White, Queen) + chessboard.get_pieces(Black, Queen))) * 24;

    //int material = std::min(game_phase, 24);

    return network.evaluate<color>() * (68 + game_phase) / 74;
    */
    return network.evaluate<color>();
}

void set_position(board& chessboard) {
    network.refresh();
    int wking = lsb(chessboard.get_pieces(White, King));
    int bking = lsb(chessboard.get_pieces(Black, King));

    for (Color side : {White, Black}) {
        for (Piece piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = chessboard.get_pieces(side, piece);

            while (bitboard) {
                Square square = pop_lsb(bitboard);
                network.update_accumulator<Operation::Set>(piece, side, square, wking, bking);
            }
        }
    }
}

template<Color color>
void update_bucket(board& chessboard, int wking, int bking) {
    network.refresh_current_accumulator();

    for (Color side : {White, Black}) {
        for (Piece piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = chessboard.get_pieces(side, piece);

            while (bitboard) {
                Square square = pop_lsb(bitboard);
                network.update_accumulator<Operation::Set>(piece, side, square, wking, bking);
            }
        }
    }
}

template<Color our_color, bool make>
void set_piece(board& chessboard, const Square square, const Piece piece, int wking = 0, int bking = 0) {
    chessboard.set_piece<our_color, make>(square, piece);

    if constexpr (make) {
        network.update_accumulator<Operation::Set>(piece, our_color, square, wking, bking);
    }
}

template<Color our_color, bool make>
void unset_piece(board& chessboard, const Square square, const Piece piece, int wking = 0, int bking = 0) {
    chessboard.unset_piece<our_color, make>(square, piece);

    if constexpr (make) {
        network.update_accumulator<Operation::Unset>(piece, our_color, square, wking, bking);
    }
}

template<Color our_color, Color their_color, bool make>
void replace_piece(board& chessboard, const Square square, const Piece piece, const Piece captured_piece, int wking = 0, int bking = 0) {
    chessboard.replace_piece<our_color, their_color, make>(square, piece, captured_piece);

    if constexpr (make) {
        network.update_accumulator<Operation::Set>(piece, our_color, square, wking, bking);
        network.update_accumulator<Operation::Unset>(captured_piece, their_color, square, wking, bking);
    }
}

template<Color our_color, bool update = true>
void make_move(board& chessboard, const chess_move move) {
    chessboard.increment_fifty_move_clock();
    chessboard.update_board_hash();

    chessboard.set_enpassant(Square::Null_Square);

    if constexpr (update) {
        network.push();
    }

    constexpr Color their_color = (our_color == White) ? Black : White;
    constexpr Direction down = (our_color == White) ? SOUTH : NORTH;
    constexpr Square our_A_square = (our_color == White) ? A1 : A8;
    constexpr Square our_H_square = (our_color == White) ? H1 : H8;
    constexpr Square our_C_square = (our_color == White) ? C1 : C8;
    constexpr Square our_D_square = (our_color == White) ? D1 : D8;
    constexpr Square our_F_square = (our_color == White) ? F1 : F8;
    constexpr Square our_G_square = (our_color == White) ? G1 : G8;
    constexpr Square king_castle_square = (our_color == White) ? E1 : E8;

    Square square_from = move.get_from();
    const Square square_to = move.get_to();
    const MoveType movetype = move.get_move_type();
    const Piece piece = chessboard.get_piece(square_from);
    const Piece captured_piece = chessboard.get_piece(square_to);

    int wking = lsb(chessboard.get_pieces(White, King));
    int bking = lsb(chessboard.get_pieces(Black, King));

    if (piece == King && buckets[square_from] != buckets[square_to]) {
        if constexpr (our_color == White) {
            wking = square_to;
        }
        else
        {
            bking = square_to;
        }
        update_bucket<our_color>(chessboard, wking, bking);
    }

    unset_piece<our_color, true>(chessboard, square_from, piece, wking, bking);
    switch (movetype) {
        case QUIET:
            if (piece == Pawn) chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, piece, wking, bking);
            break;
        case DOUBLE_PAWN_PUSH:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, Pawn, wking, bking);
            chessboard.set_enpassant(square_to + down);
            break;
        case KING_CASTLE:
            square_from = king_castle_square;
            unset_piece<our_color, true>(chessboard, our_H_square, Rook, wking, bking);
            set_piece<our_color, true>(chessboard, our_F_square, Rook, wking, bking);
            set_piece<our_color, true>(chessboard, our_G_square, King, wking, bking);
            break;
        case QUEEN_CASTLE:
            square_from = king_castle_square;
            unset_piece<our_color, true>(chessboard, our_A_square, Rook, wking, bking);
            set_piece<our_color, true>(chessboard, our_D_square, Rook, wking, bking);
            set_piece<our_color, true>(chessboard, our_C_square, King, wking, bking);
            break;
        case CAPTURE:
            chessboard.reset_fifty_move_clock();
            replace_piece<our_color, their_color, true>(chessboard, square_to, piece, captured_piece, wking, bking);
            chessboard.update_castling_rights(square_to);
            break;
        case EN_PASSANT:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, piece, wking, bking);
            unset_piece<their_color, true>(chessboard, square_to + down, piece, wking, bking);
            break;
        case KNIGHT_PROMOTION:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, Knight, wking, bking);
            break;
        case BISHOP_PROMOTION:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, Bishop, wking, bking);
            break;
        case ROOK_PROMOTION:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, Rook, wking, bking);
            break;
        case QUEEN_PROMOTION:
            chessboard.reset_fifty_move_clock();
            set_piece<our_color, true>(chessboard, square_to, Queen, wking, bking);
            break;
        case KNIGHT_PROMOTION_CAPTURE:
            chessboard.reset_fifty_move_clock();
            replace_piece<our_color, their_color, true>(chessboard, square_to, Knight, captured_piece, wking, bking);
            chessboard.update_castling_rights(square_to);
            break;
        case BISHOP_PROMOTION_CAPTURE:
            chessboard.reset_fifty_move_clock();
            replace_piece<our_color, their_color, true>(chessboard, square_to, Bishop, captured_piece, wking, bking);
            chessboard.update_castling_rights(square_to);
            break;
        case ROOK_PROMOTION_CAPTURE:
            chessboard.reset_fifty_move_clock();
            replace_piece<our_color, their_color, true>(chessboard, square_to, Rook, captured_piece, wking, bking);
            chessboard.update_castling_rights(square_to);
            break;
        case QUEEN_PROMOTION_CAPTURE:
            chessboard.reset_fifty_move_clock();
            replace_piece<our_color, their_color, true>(chessboard, square_to, Queen, captured_piece, wking, bking);
            chessboard.update_castling_rights(square_to);
            break;
    }

    chessboard.update_castling_rights(square_from);
    chessboard.set_side(their_color);
    chessboard.emplace_history(captured_piece, move);
}

template <Color our_color, bool update = true>
void undo_move(board& chessboard) {
    if constexpr (update) {
        network.pull();
    }

    board_info b_info = chessboard.get_history();
    const chess_move played_move = b_info.move;
    const Piece captured_piece = b_info.captured_piece;

    chessboard.set_side(our_color);

    constexpr Color their_color = (our_color) == White ? Black : White;
    constexpr Direction down = (our_color == White) ? SOUTH : NORTH;

    constexpr Square our_rook_A_square = (our_color == White) ? A1 : A8;
    constexpr Square our_rook_H_square = (our_color == White) ? H1 : H8;
    constexpr Square our_C_square = (our_color == White) ? C1 : C8;
    constexpr Square our_D_square = (our_color == White) ? D1 : D8;
    constexpr Square our_F_square = (our_color == White) ? F1 : F8;
    constexpr Square our_G_square = (our_color == White) ? G1 : G8;
    constexpr Square king_castle_square = (our_color == White) ? E1 : E8;

    const Square square_from = played_move.get_from();
    const Square square_to = played_move.get_to();
    const MoveType movetype = played_move.get_move_type();
    const Piece piece = chessboard.get_piece(square_to);

    switch (movetype) {
        case QUIET:
        case DOUBLE_PAWN_PUSH:
            set_piece<our_color, false>(chessboard, square_from, piece);
            unset_piece<our_color, false>(chessboard, square_to, piece);
            break;
        case KING_CASTLE:
            set_piece<our_color, false>(chessboard, king_castle_square, King);
            set_piece<our_color, false>(chessboard, our_rook_H_square, Rook);
            unset_piece<our_color, false>(chessboard, our_F_square, Rook);
            unset_piece<our_color, false>(chessboard, our_G_square, King);
            break;
        case QUEEN_CASTLE:
            set_piece<our_color, false>(chessboard, king_castle_square, King);
            set_piece<our_color, false>(chessboard, our_rook_A_square, Rook);
            unset_piece<our_color, false>(chessboard, our_D_square, Rook);
            unset_piece<our_color, false>(chessboard, our_C_square, King);
            break;
        case CAPTURE:
            set_piece<our_color, false>(chessboard, square_from, piece);
            replace_piece<their_color, our_color, false>(chessboard, square_to, captured_piece, piece);
            break;
        case EN_PASSANT:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            unset_piece<our_color, false>(chessboard, square_to, Pawn);
            set_piece<their_color, false>(chessboard, square_to + down, Pawn);
            break;
        case KNIGHT_PROMOTION:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            unset_piece<our_color, false>(chessboard, square_to, Knight);
            break;
        case BISHOP_PROMOTION:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            unset_piece<our_color, false>(chessboard, square_to, Bishop);
            break;
        case ROOK_PROMOTION:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            unset_piece<our_color, false>(chessboard, square_to, Rook);
            break;
        case QUEEN_PROMOTION:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            unset_piece<our_color, false>(chessboard, square_to, Queen);
            break;
        case KNIGHT_PROMOTION_CAPTURE:
        case BISHOP_PROMOTION_CAPTURE:
        case ROOK_PROMOTION_CAPTURE:
        case QUEEN_PROMOTION_CAPTURE:
            set_piece<our_color, false>(chessboard, square_from, Pawn);
            replace_piece<their_color, our_color, false>(chessboard, square_to, captured_piece, piece);
            break;
    }

    chessboard.undo_history();
}

#endif //MOTOR_MAKEMOVE_HPP
