#ifndef MOTOR_SEE_HPP
#define MOTOR_SEE_HPP

#include <cstdint>
#include "../../chess_board/board.hpp"

constexpr std::int32_t SEE_VALUES[7] = {100, 450, 450, 650, 1250, 30'000, 0};

template <Color color>
static bool see(board & chessboard, chess_move & capture, int threshold = 0)  {
    Square from = capture.get_from();
    Square to   = capture.get_to();

    Piece piece = chessboard.get_piece(from);
    if (piece == Pawn || piece == King) {
        return true;
    }

    int side_to_capture = color ^ 1;

    Piece captured_piece = chessboard.get_piece(to);
    int score = SEE_VALUES[captured_piece] - SEE_VALUES[piece] - threshold;

    // winning capture
    if (score >= 0) {
        return true;
    }

    uint64_t queens  = chessboard.get_pieces(White, Queen)  | chessboard.get_pieces(Black, Queen);
    uint64_t rooks   = chessboard.get_pieces(White, Rook)   | chessboard.get_pieces(Black, Rook);
    uint64_t bishops = chessboard.get_pieces(White, Bishop) | chessboard.get_pieces(Black, Bishop);
    rooks   |= queens;
    bishops |= queens;

    uint64_t occupancy = chessboard.get_occupancy() ^ (1ull << from) ^ (1ull << to) ;

    uint64_t attackers = chessboard.attackers<White>(to) | chessboard.attackers<Black>(to);

    uint64_t side_occupancy[2] = {chessboard.get_side_occupancy<White>(), chessboard.get_side_occupancy<Black>()};

    while (true) {
        //attackers &= side_occupancy[side_to_capture];
        if ((attackers & side_occupancy[side_to_capture]) == 0ull) {
            break;
        }

        for (Piece pt : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            uint64_t bitboard = attackers & chessboard.get_pieces(side_to_capture, pt);
            if (bitboard) {
                occupancy ^= (1ull << lsb(bitboard));
                piece = pt;
            }
        }

        score = -score - 1 - SEE_VALUES[piece];

        side_to_capture ^= 1;

        if (score >= 0) {
            if (piece == King && attackers) {
                return side_to_capture == color;
            }
            break;
        }


        if (piece == Pawn || piece == Bishop || piece == Queen) {
            attackers |= (attacks<Ray::BISHOP>(to, occupancy) & bishops);
        }

        if (piece == Rook || piece == Queen) {
            attackers |= (attacks<Ray::ROOK>(to, occupancy) & rooks);
        }

        attackers &= occupancy;
    }

    return color != side_to_capture;
}

#endif //MOTOR_SEE_HPP
