#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include "../chess_board/board.hpp"
#include <algorithm>

#include "incbin.hpp"
#include <immintrin.h> // Include for AVX2


constexpr unsigned int HIDDEN_SIZE = 32;

struct Weights {
    std::array<std::array<std::array<std::array<std::int16_t, HIDDEN_SIZE>, 64>, 6>, 2> feature_weight;
    std::array<std::int16_t, HIDDEN_SIZE>  feature_bias;
    std::array<std::int16_t, HIDDEN_SIZE> output_weight_STM;
    std::array<std::int16_t, HIDDEN_SIZE> output_weight_NSTM;
    std::int16_t output_bias;
};

INCBIN(Weights, "nnue.bin");
const Weights& weights = *reinterpret_cast<const Weights*>(gWeightsData);

enum class Operation {
    Set, Unset
};

template<std::uint16_t hidden_size>
class perspective_network
{
public:
    std::array<std::array<std::int16_t, hidden_size>, 512> white_accumulator_stack;
    std::array<std::array<std::int16_t, hidden_size>, 512> black_accumulator_stack;
    unsigned int index;

    perspective_network() {
        refresh();
    }

    void refresh() {
        white_accumulator_stack[0] = black_accumulator_stack[0] = weights.feature_bias;
        index = 0;
    }

    void push() {
        white_accumulator_stack[index + 1] = white_accumulator_stack[index];
        black_accumulator_stack[index + 1] = black_accumulator_stack[index];
        index++;
    }

    void pull() {
        index--;
    }

    template<Operation operation>
    void update_accumulator(const Piece piece, const Color color, const Square square) {
        const auto& white_weights = weights.feature_weight[color][piece][square];
        const auto& black_weights = weights.feature_weight[color ^ 1][piece][square ^ 56];

        auto& white_accumulator = white_accumulator_stack[index];
        auto& black_accumulator = black_accumulator_stack[index];

        if constexpr (operation == Operation::Set) {
            for (std::size_t i = 0; i < hidden_size; i++) {
                white_accumulator[i] += white_weights[i];
                black_accumulator[i] += black_weights[i];
            }
        }
        else {
            for (std::size_t i = 0; i < hidden_size; i++) {
                white_accumulator[i] -= white_weights[i];
                black_accumulator[i] -= black_weights[i];
            }
        }
    }

    template <Color color>
    std::int32_t evaluate() {
        constexpr std::int16_t min_value = 0, max_value = 255;
        std::int32_t sum = weights.output_bias;

        const auto& stm_accumulator = color == White ? white_accumulator_stack[index] : black_accumulator_stack[index];
        const auto& nstm_accumulator = color == White ? black_accumulator_stack[index] : white_accumulator_stack[index];

        for (std::size_t j = 0; j < hidden_size; j++) {
            sum += std::clamp(stm_accumulator[j], min_value, max_value) * weights.output_weight_STM[j];
            sum += std::clamp(nstm_accumulator[j], min_value, max_value) * weights.output_weight_NSTM[j];
        }
        
        return sum * 400 / 16320;
    }
};

perspective_network<HIDDEN_SIZE> network;

void set_position(board& chessboard) {
    network.refresh();

    for (Color side : {White, Black}) {
        for (Piece piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = chessboard.get_pieces(side, piece);

            while (bitboard) {
                Square square = pop_lsb(bitboard);
                network.update_accumulator<Operation::Set>(piece, side, square);
            }
        }
    }
}

template<Color our_color, bool make>
void set_piece(board & chessboard, const Square square, const Piece piece) {
    chessboard.set_piece<our_color, make>(square, piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Set>(piece, our_color, square);
    }
}

template<Color our_color, bool make>
void unset_piece(board& chessboard, const Square square, const Piece piece) {
    chessboard.unset_piece<our_color, make>(square, piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Unset>(piece, our_color, square);
    }
}

template<Color our_color, Color their_color, bool make>
void replace_piece(board & chessboard, const Square square, const Piece piece, const Piece captured_piece) {
    chessboard.replace_piece<our_color, their_color, make>(square, piece, captured_piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Set>(piece, our_color, square);
        network.update_accumulator<Operation::Unset>(captured_piece, their_color, square);
    }
}

template<Color our_color>
void make_move(board & chessboard, const chess_move move) {
    chessboard.increment_fifty_move_clock();
    chessboard.update_board_hash();

    chessboard.set_enpassant(Square::Null_Square);

    network.push();

    constexpr Color their_color = (our_color == White) ? Black : White;
    constexpr Direction down = (our_color == White) ? SOUTH : NORTH;
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

    unset_piece<our_color, true>(chessboard, square_from, piece);
    switch (movetype) {
    case QUIET:
        if (piece == Pawn) chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, piece);
        break;
    case DOUBLE_PAWN_PUSH:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, Pawn);
        chessboard.set_enpassant(square_to + down);
        break;
    case KING_CASTLE:
        square_from = king_castle_square;
        unset_piece<our_color, true>(chessboard, king_castle_square, King);
        set_piece<our_color, true>(chessboard, our_F_square, Rook);
        set_piece<our_color, true>(chessboard, our_G_square, King);
        break;
    case QUEEN_CASTLE:
        square_from = king_castle_square;
        unset_piece<our_color, true>(chessboard, king_castle_square, King);
        set_piece<our_color, true>(chessboard, our_D_square, Rook);
        set_piece<our_color, true>(chessboard, our_C_square, King);
        break;
    case CAPTURE:
        chessboard.reset_fifty_move_clock();
        replace_piece<our_color, their_color, true>(chessboard, square_to, piece, captured_piece);
        chessboard.update_castling_rights(square_to);
        break;
    case EN_PASSANT:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, piece);
        unset_piece<their_color, true>(chessboard, square_to + down, piece);
        break;
    case KNIGHT_PROMOTION:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, Knight);
        break;
    case BISHOP_PROMOTION:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, Bishop);
        break;
    case ROOK_PROMOTION:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, Rook);
        break;
    case QUEEN_PROMOTION:
        chessboard.reset_fifty_move_clock();
        set_piece<our_color, true>(chessboard, square_to, Queen);
        break;
    case KNIGHT_PROMOTION_CAPTURE:
        chessboard.reset_fifty_move_clock();
        replace_piece<our_color, their_color, true>(chessboard, square_to, Knight, captured_piece);
        chessboard.update_castling_rights(square_to);
        break;
    case BISHOP_PROMOTION_CAPTURE:
        chessboard.reset_fifty_move_clock();
        replace_piece<our_color, their_color, true>(chessboard, square_to, Bishop, captured_piece);
        chessboard.update_castling_rights(square_to);
        break;
    case ROOK_PROMOTION_CAPTURE:
        chessboard.reset_fifty_move_clock();
        replace_piece<our_color, their_color, true>(chessboard, square_to, Rook, captured_piece);
        chessboard.update_castling_rights(square_to);
        break;
    case QUEEN_PROMOTION_CAPTURE:
        chessboard.reset_fifty_move_clock();
        replace_piece<our_color, their_color, true>(chessboard, square_to, Queen, captured_piece);
        chessboard.update_castling_rights(square_to);
        break;
    }

    chessboard.update_castling_rights(square_from);
    chessboard.set_side(their_color);
    chessboard.emplace_history(captured_piece, move);
}

template <Color our_color>
void undo_move(board & chessboard) {
    network.pull();

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

#endif //MOTOR_NNUE_HPP