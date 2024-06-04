#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include "../chess_board/board.hpp"
#include <algorithm>

#include "incbin.hpp"

#include <immintrin.h>

constexpr unsigned int HIDDEN_SIZE = 1024;
constexpr int QA = 403;
constexpr int QB = 64;

constexpr std::array<int, 64> buckets = {
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1,
};

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

int screlu(int x) {
    std::int16_t clamped = std::clamp(x, 0, QA);
    return clamped * clamped;
}

template<std::uint16_t hidden_size>
class perspective_network
{
public:
    std::array<std::array<std::int16_t, hidden_size>, 128> white_accumulator_stack;
    std::array<std::array<std::int16_t, hidden_size>, 128> black_accumulator_stack;
    unsigned int index;

    perspective_network() {
        refresh();
    }

    void refresh_current_accumulator() {
        white_accumulator_stack[index] = black_accumulator_stack[index] = weights.feature_bias;
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

    int get_square_index(int square, int king_square) {
        return (king_square % 8 > 3) ? square ^ 7 : square;
    }

    template<Operation operation>
    void update_accumulator(const Piece piece, const Color color, const Square square, int wking, int bking) {
        const auto& white_weights = weights.feature_weight[color][piece][get_square_index(square, wking)];
        const auto& black_weights = weights.feature_weight[color ^ 1][piece][get_square_index(square, bking) ^ 56];

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

#ifndef __AVX2__ 
    template <Color color>
    std::int32_t evaluate() {
        std::int32_t sum = 0;

        const auto& stm_accumulator = color == White ? white_accumulator_stack[index] : black_accumulator_stack[index];
        const auto& nstm_accumulator = color == White ? black_accumulator_stack[index] : white_accumulator_stack[index];

        for (std::size_t j = 0; j < hidden_size; j++) {
            sum += screlu(stm_accumulator[j]) * weights.output_weight_STM[j];
            sum += screlu(nstm_accumulator[j]) * weights.output_weight_NSTM[j];
        }

        return (sum / QA + weights.output_bias) * 400 / (QB * QA);
    }
#else
    template <Color color>
    std::int32_t evaluate() {
        const auto& stm_accumulator = color == White ? white_accumulator_stack[index] : black_accumulator_stack[index];
        const auto& nstm_accumulator = color == White ? black_accumulator_stack[index] : white_accumulator_stack[index];

        std::int32_t sum = 0;
        sum += flatten(stm_accumulator.data(), weights.output_weight_STM.data());
        sum += flatten(nstm_accumulator.data(), weights.output_weight_NSTM.data());  
        
        return (sum / QA + weights.output_bias) * 400 / (QB * QA);
    }

private:
    std::int32_t flatten(const std::int16_t * accumulator, const std::int16_t * weights) {
        constexpr int CHUNK = 16;
        auto sum = _mm256_setzero_si256();
        auto min = _mm256_setzero_si256();
        auto max = _mm256_set1_epi16(QA);
        for (int i = 0; i < hidden_size / CHUNK ; i++) {
            auto us_vector = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(accumulator + i * CHUNK));
            auto weights_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i * CHUNK));
            auto clamped = _mm256_min_epi16(_mm256_max_epi16(us_vector, min), max);
            auto mul = _mm256_madd_epi16(clamped, _mm256_mullo_epi16(clamped, weights_vec));
            sum = _mm256_add_epi32(sum, mul);
        }
        return horizontal_sum(sum);
    }

    std::int32_t horizontal_sum(const __m256i input_sum) {
        __m256i horizontal_sum_256 = _mm256_hadd_epi32(input_sum, input_sum);
        __m128i upper_128 = _mm256_extracti128_si256(horizontal_sum_256, 1);
        __m128i combined_128 = _mm_add_epi32(upper_128, _mm256_castsi256_si128(horizontal_sum_256));
        __m128i horizontal_sum_128 = _mm_hadd_epi32(combined_128, combined_128);
        return _mm_cvtsi128_si32(horizontal_sum_128);
    }
#endif // __AVX2__
};

perspective_network<HIDDEN_SIZE> network;

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
void set_piece(board & chessboard, const Square square, const Piece piece, int wking = 0, int bking = 0) {
    chessboard.set_piece<our_color, make>(square, piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Set>(piece, our_color, square, wking, bking);
    }
}

template<Color our_color, bool make>
void unset_piece(board& chessboard, const Square square, const Piece piece, int wking = 0, int bking = 0) {
    chessboard.unset_piece<our_color, make>(square, piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Unset>(piece, our_color, square, wking, bking);
    }
}

template<Color our_color, Color their_color, bool make>
void replace_piece(board & chessboard, const Square square, const Piece piece, const Piece captured_piece, int wking = 0, int bking = 0) {
    chessboard.replace_piece<our_color, their_color, make>(square, piece, captured_piece);
    
    if constexpr (make == true) {
        network.update_accumulator<Operation::Set>(piece, our_color, square, wking, bking);
        network.update_accumulator<Operation::Unset>(captured_piece, their_color, square, wking, bking);
    }
}

template<Color our_color, bool update = true>
void make_move(board & chessboard, const chess_move move) {
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
void undo_move(board & chessboard) {
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

#endif //MOTOR_NNUE_HPP
