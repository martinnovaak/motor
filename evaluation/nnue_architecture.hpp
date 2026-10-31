#ifndef MOTOR_NNUE_ARCHITECTURE_HPP
#define MOTOR_NNUE_ARCHITECTURE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

constexpr unsigned int HIDDEN_SIZE = 16;
constexpr int QA = 403;
constexpr int QB = 81;
constexpr int Buckets = 1;
constexpr bool HORIZONTAL_MIRRORING = false;
constexpr int NNUE_FEATURES = Buckets * 2 * 6 * 64;

constexpr std::array<int, 64> buckets{};

constexpr std::size_t nnue_feature_index(int bucket, int color, int piece,
                                         int square) {
    return (((bucket * 2 + color) * 6 + piece) * 64 + square);
}

struct Weights {
    std::array<std::array<std::int16_t, HIDDEN_SIZE>, NNUE_FEATURES> feature_weight;
    std::array<std::int16_t, HIDDEN_SIZE> feature_bias;
    std::array<std::int16_t, HIDDEN_SIZE> output_weight_STM;
    std::array<std::int16_t, HIDDEN_SIZE> output_weight_NSTM;
    std::int16_t output_bias;
};

#endif // MOTOR_NNUE_ARCHITECTURE_HPP
