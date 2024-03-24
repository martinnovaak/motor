#ifndef MOTOR_SEE_HPP
#define MOTOR_SEE_HPP

#include <cstdint>
#include "../../chess_board/board.hpp"

constexpr std::int32_t SEE_VALUES[7] = { 100, 300, 300, 500, 900, 30'000, 0 };

template <Color color>
static bool see(board& chessboard, const chess_move& chessmove, int threshold = 0) {
    Square from = chessmove.get_from();
    Square to = chessmove.get_to();

    int balance = -threshold;

    if (!chessmove.is_quiet()) {
        Piece captured_piece = chessboard.get_piece(to);
        balance += SEE_VALUES[captured_piece];
        if (balance < 0) {
            return false;
        }
    }

    balance = SEE_VALUES[chessboard.get_piece(from)] - balance;
    if (balance <= 0) {
        return true;
    }

    int side_to_capture = color ^ 1;

    const std::uint64_t queens = chessboard.get_pieces(White, Queen) | chessboard.get_pieces(Black, Queen);
    const std::uint64_t rooks = chessboard.get_pieces(White, Rook) | chessboard.get_pieces(Black, Rook) | queens;
    const std::uint64_t bishops = chessboard.get_pieces(White, Bishop) | chessboard.get_pieces(Black, Bishop) | queens;

    std::uint64_t occupancy = chessboard.get_occupancy() ^ (1ull << from) ^ (1ull << to);

    std::uint64_t attackers = chessboard.attackers<White>(to, occupancy) | chessboard.attackers<Black>(to, occupancy);

    std::uint64_t side_occupancy[2] = { chessboard.get_side_occupancy<White>(), chessboard.get_side_occupancy<Black>() };


    Piece piece;
    while (true) {
        //attackers &= side_occupancy[side_to_capture];
        if ((attackers & side_occupancy[side_to_capture]) == 0ull) {
            break;
        }

        for (Piece pt : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = attackers & chessboard.get_pieces(side_to_capture, pt);
            if (bitboard) {
                occupancy ^= (1ull << lsb(bitboard));
                piece = pt;
                break;
            }
        }

        balance = -balance - 1 - SEE_VALUES[piece];

        side_to_capture ^= 1;

        if (balance >= 0) {
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
