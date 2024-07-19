#ifndef MOTOR_MAKEMOVE_HPP
#define MOTOR_MAKEMOVE_HPP

#include "../chess_board/board.hpp"
#include "../evaluation/nnue.hpp"

template <Color color>
std::int16_t evaluate(board& chessboard) {
    int game_phase =
            (popcount(chessboard.get_pieces(White, Knight) + chessboard.get_pieces(Black, Knight))) +
            (popcount(chessboard.get_pieces(White, Bishop) + chessboard.get_pieces(Black, Bishop))) +
            (popcount(chessboard.get_pieces(White, Rook) + chessboard.get_pieces(Black, Rook))) * 2 +
            (popcount(chessboard.get_pieces(White, Queen) + chessboard.get_pieces(Black, Queen))) * 4;

    int material = std::min(game_phase, 24);

    return network.evaluate<color>() * (56 + material) / 64;
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
    network.refresh_current_accumulator<color>();
    int king = color == White ? wking : bking;

    for (Color side : {White, Black}) {
        for (Piece piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = chessboard.get_pieces(side, piece);

            while (bitboard) {
                const Square square = pop_lsb(bitboard);
                network.add_to_accumulator<color>(piece, side, square, king);
            }
        }
    }
}

template<Color side, bool update_nnue>
void unset_piece(board & b, Piece piece, Square to, int wking, int bking) {
    b.unset_piece<side>(piece, to);

    if constexpr (update_nnue) {
        network.update_accumulator<Operation::Unset>(piece, side, to, wking, bking);
    }
}

template<Color side, bool update_nnue>
void set_piece(board & b, Piece piece, Square to, int wking, int bking) {
    b.set_piece<side>(piece, to);

    if constexpr (update_nnue) {
        network.update_accumulator<Operation::Set>(piece, side, to, wking, bking);
    }
}

template<Color side, bool update_nnue>
void move_piece(board & b, Piece piece, Square from, Square to, int wking, int bking) {
    b.move_piece<side>(piece, from, to);

    if constexpr (update_nnue) {
        network.update_accumulator<Operation::Unset>(piece, side, from, wking, bking);
        network.update_accumulator<Operation::Set>(piece, side, to, wking, bking);
    }
}

template<Color side, bool update_nnue = true>
void make_move(board & b, chess_move m) {
    constexpr Color their_side = side == White ? Black : White;
    constexpr Direction PawnDirection = side == White ? NORTH : SOUTH;
    const Square from = m.get_from();
    const Square to = m.get_to();
    const Piece piece = b.get_piece(from);
    const Piece capture = b.get_piece(to);

    b.make_state<their_side>(capture, m);
    b.update_castling_rights(from);

    int wking = lsb(b.get_pieces(White, King));
    int bking = lsb(b.get_pieces(Black, King));

    if constexpr (update_nnue) {
        network.push();
    }

    switch(m.get_move_type()) {
        case NORMAL: {
            if (capture != Null_Piece) {
                b.update_hash(their_side, capture, to);
                unset_piece<their_side, update_nnue>(b, b.get_piece(to), to, wking, bking);
                b.reset_fifty_move_clock();
                b.update_castling_rights(to);
            }

            b.update_hash(side, piece, from);
            b.update_hash(side, piece, to);
            move_piece<side, update_nnue>(b, b.get_piece(from), from, to, wking, bking);

            if (piece == Pawn) {
                b.reset_fifty_move_clock();

                if ((int(from) ^ int(to)) == int(NORTH_2)) {
                    const Square epsq = to - PawnDirection;

                    if (PAWN_ATTACKS_TABLE[side][epsq] & b.get_pieces(their_side, Pawn)) {
                        b.set_enpassant(epsq);
                    }
                }
            }
            break;
        }
        case CASTLING: {
            CastlingRight kingside = side == White ? CASTLE_WHITE_KINGSIDE : CASTLE_BLACK_KINGSIDE;
            CastlingRight queenside = side == White ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;

            CastlingRight cr = to > from ? kingside : queenside;
            const Square rookFrom = CastlingRookFrom[cr];
            const Square rookTo = CastlingRookTo[cr];

            b.update_hash(side, King, from);
            b.update_hash(side, King, to);
            b.update_hash(side, Rook, rookFrom);
            b.update_hash(side, Rook, rookTo);
            move_piece<side, update_nnue>(b, King, from, to, wking, bking);
            move_piece<side, update_nnue>(b, Rook, rookFrom, rookTo, wking, bking);

            break;
        }
        case PROMOTION:  {
            const Piece promotionType = m.get_promotion();
            if (capture != Null_Piece) {
                b.update_hash(their_side, capture, to);
                unset_piece<their_side, update_nnue>(b, b.get_piece(to), to, wking, bking);
                b.update_castling_rights(to);
            }

            b.update_hash(side, Pawn, from);
            b.update_hash(side, promotionType, to);
            unset_piece<side, update_nnue>(b, Pawn, from, wking, bking);
            set_piece<side, update_nnue>(b, promotionType, to, wking, bking);
            break;
        }
        case EN_PASSANT: {
            const Square epsq = to - PawnDirection;
            b.update_hash(their_side, Pawn, epsq);
            b.update_hash(side, Pawn, from);
            b.update_hash(side, Pawn, to);
            unset_piece<their_side, update_nnue>(b, Pawn, epsq, wking, bking);
            move_piece<side, update_nnue>(b, Pawn, from, to, wking, bking);

            b.reset_fifty_move_clock();
            break;
        }
    }

    if constexpr (update_nnue) {
        if (piece == King && (((side == White) && buckets[from] != buckets[to]) || ((side == Black) && buckets[from ^ 56] != buckets[to ^ 56]))) {
            if constexpr (side == White) {
                wking = to;
            } else {
                bking = to;
            }
            update_bucket<side>(b, wking, bking);
        }
    }

    b.update_bitboards<their_side>();
}

template<Color side, bool update_nnue = true>
void undo_move(board & b, chess_move m) {
    constexpr Color their_side = side == White ? Black : White;
    const Square from = m.get_from();
    const Square to = m.get_to();
    const Piece capture = b.get_captured_piece();

    if constexpr (update_nnue) {
        network.pull();
    }

    b.undo_state<side>();

    switch(m.get_move_type()) {
        case NORMAL: {
            b.move_piece<side>(b.get_piece(to), to, from);

            if (capture != Null_Piece) {
                b.set_piece<their_side>(capture, to);
            }
            return;
        }
        case CASTLING: {
            constexpr CastlingRight kingside = side == White ? CASTLE_WHITE_KINGSIDE : CASTLE_BLACK_KINGSIDE;
            constexpr CastlingRight queenside = side == White ? CASTLE_WHITE_QUEENSIDE : CASTLE_BLACK_QUEENSIDE;

            CastlingRight cr = to > from ? kingside : queenside;
            const Square rookFrom = CastlingRookFrom[cr];
            const Square rookTo = CastlingRookTo[cr];

            b.move_piece<side>(King, to, from);
            b.move_piece<side>(Rook, rookTo, rookFrom);
            return;
        }
        case PROMOTION: {
            b.unset_piece<side>(b.get_piece(to), to);
            b.set_piece<side>(Pawn, from);

            if (capture != Null_Piece) {
                b.set_piece<their_side>(capture, to);
            }
            return;
        }
        case EN_PASSANT: {
            b.move_piece<side>(Pawn, to, from);
            b.set_piece<their_side>(Pawn, Square(int(to) ^ 8));
            return;
        }
    }
}

#endif //MOTOR_MAKEMOVE_HPP
