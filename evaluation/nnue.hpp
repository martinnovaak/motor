#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include <algorithm>

#include "incbin.hpp"

#include <immintrin.h>

constexpr unsigned int HIDDEN_SIZE = 1536;
constexpr int QA = 403;
constexpr int QB = 81;

constexpr std::array<int, 64> buckets = {
        0, 0, 1, 1, 4, 4, 3, 3,
        0, 0, 1, 1, 4, 4, 3, 3,
        2, 2, 2, 2, 5, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
};

struct Weights {
    std::array<std::array<std::array<std::array<std::array<std::int16_t, HIDDEN_SIZE>, 64>, 6>, 2>, 3> feature_weight;
    std::array<std::int16_t, HIDDEN_SIZE> feature_bias;
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
struct accumulator_cache {
    std::array<std::int16_t, hidden_size> accumulator;
    std::array<Piece, 64> pieces;
    std::uint64_t occupancy;
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

    template <Color perspective>
    void refresh_current_accumulator() {
        if constexpr (perspective == White) {
            white_accumulator_stack[index] = weights.feature_bias;
        } else {
            black_accumulator_stack[index] = weights.feature_bias;
        }
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

    template<Operation operation, Color perspective>
    void update_accumulator(const Piece piece, const Color color, const Square square, int king) {
        if constexpr (perspective == White) {
            const auto& white_weights = weights.feature_weight[buckets[king] % 3][color][piece][get_square_index(square, king)];
            auto& white_accumulator = white_accumulator_stack[index];

            if constexpr (operation == Operation::Set) {
                for (std::size_t i = 0; i < hidden_size; i++) {
                    white_accumulator[i] += white_weights[i];
                }
            }
            else {
                for (std::size_t i = 0; i < hidden_size; i++) {
                    white_accumulator[i] -= white_weights[i];
                }
            }
        } else {
            const auto& black_weights = weights.feature_weight[buckets[king ^ 56] % 3][color ^ 1][piece][get_square_index(square, king) ^ 56];
            auto& black_accumulator = black_accumulator_stack[index];

            if constexpr (operation == Operation::Set) {
                for (std::size_t i = 0; i < hidden_size; i++) {
                    black_accumulator[i] += black_weights[i];
                }
            }
            else {
                for (std::size_t i = 0; i < hidden_size; i++) {
                    black_accumulator[i] -= black_weights[i];
                }
            }
        }
    }

    template<Operation operation>
    void update_accumulator(const Piece piece, const Color color, const Square square, int wking, int bking) {
        const auto& white_weights = weights.feature_weight[buckets[wking] % 3][color][piece][get_square_index(square, wking)];
        const auto& black_weights = weights.feature_weight[buckets[bking ^ 56] % 3][color ^ 1][piece][get_square_index(square, bking) ^ 56];

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
    std::int32_t flatten(const std::int16_t* accumulator, const std::int16_t* weights) {
        constexpr int CHUNK = 16;
        auto sum = _mm256_setzero_si256();
        auto min = _mm256_setzero_si256();
        auto max = _mm256_set1_epi16(QA);
        for (int i = 0; i < hidden_size / CHUNK; i++) {
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

#endif //MOTOR_NNUE_HPP