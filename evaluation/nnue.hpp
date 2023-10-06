#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include "weights.hpp"
#include "../chess_board/board.hpp"

enum class Operation {
    Set, Unset
};

template<std::uint16_t HiddenSize>
class perspective_network
{
public:
    std::array<std::int16_t, HiddenSize> white_accumulator;
    std::array<std::int16_t, HiddenSize> black_accumulator;

    perspective_network() {
        refresh();
    }

    void refresh() {
        white_accumulator = black_accumulator = feature_bias;
    }

    template<Operation operation>
    void update_accumulator(const Piece piece, const Color color, const Square square) {
        const auto & white_weights = feature_weight[color][piece][square];
        const auto & black_weights = feature_weight[color ^ 1][piece][square ^ 56];

        if constexpr (operation == Operation::Set) {
            for (std::size_t i = 0; i < HiddenSize; i++) {
                white_accumulator[i] += white_weights[i];
                black_accumulator[i] += black_weights[i];
            }
        } else {
            for (std::size_t i = 0; i < HiddenSize; i++) {
                white_accumulator[i] -= white_weights[i];
                black_accumulator[i] -= black_weights[i];
            }
        }
    }

    template <Color color>
    std::int32_t evaluate() {
        constexpr std::int16_t min_value = 0, max_value = 255;
        std::int32_t sum = output_bias;

        auto & stm_accumulator  = color == White ? white_accumulator : black_accumulator;
        auto & nstm_accumulator = color == White ? black_accumulator : white_accumulator;

        for (std::size_t j = 0; j < HiddenSize; j++) {
            sum += std::clamp(stm_accumulator[j], min_value, max_value) * output_weight_STM[j];
            sum += std::clamp(nstm_accumulator[j], min_value, max_value) * output_weight_NSTM[j];
        }

        return sum * 400 / 16320;
    }
};

perspective_network<16> network;

template <Color color>
std::int16_t nn_eval(board & chessboard) {
    network.refresh();

    for (Color side: {White, Black}) {
        for (Piece piece: {Pawn, Knight, Bishop, Rook, Queen, King}) {
            std::uint64_t bitboard = chessboard.get_pieces(side, piece);

            while (bitboard) {
                Square square = pop_lsb(bitboard);
                network.update_accumulator<Operation::Set>(piece, side, square);
            }
        }
    }

    return network.evaluate<color>();
}

#endif //MOTOR_NNUE_HPP