#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include <algorithm>

#include "incbin.hpp"


constexpr unsigned int HIDDEN_SIZE = 64;

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
    std::array<std::array<std::int16_t, hidden_size>, 128> white_accumulator_stack;
    std::array<std::array<std::int16_t, hidden_size>, 128> black_accumulator_stack;
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

#endif //MOTOR_NNUE_HPP