#ifndef MOTOR_NNUE_HPP
#define MOTOR_NNUE_HPP

#include <algorithm>
#include <array>
#include <cstdint>

#include "../chess_board/board.hpp"
#include "nnue_architecture.hpp"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

#if !defined(__has_embed)
#error "Motor requires a compiler with #embed support (Clang 19 or newer)"
#endif

#ifndef NNUE_FILE
#define NNUE_FILE "../nnue.bin"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
alignas(64) constexpr unsigned char weights_data[] = {
#embed NNUE_FILE
};
#pragma clang diagnostic pop

static_assert(sizeof(weights_data) >= sizeof(Weights), "nnue.bin is smaller than the Weights struct");
const Weights& weights = *reinterpret_cast<const Weights*>(weights_data);

enum class Operation {
    Set, Unset
};

struct feature_change {
    Piece piece = Null_Piece;
    Color color = White;
    Square square = Null_Square;
    Operation operation = Operation::Set;
};

struct accumulator_state {
    static constexpr std::size_t MAX_CHANGES = 4;

    std::array<feature_change, MAX_CHANGES> changes{};
    std::array<Square, 2> king_square{Null_Square, Null_Square};
    std::uint8_t change_count = 0;
    bool computed = false;
};

int screlu(int x) {
    std::int16_t clamped = std::clamp(x, 0, QA);
    return clamped * clamped;
}

template<std::uint16_t hidden_size>
struct accumulator_cache {
    std::array<std::array<std::int16_t, hidden_size>, 128> white_accumulator_stack;
    std::array<std::array<std::int16_t, hidden_size>, 128> black_accumulator_stack;
};

template<std::uint16_t hidden_size>
class perspective_network
{
public:
    std::array<std::array<std::int16_t, hidden_size>, 128> white_accumulator_stack;
    std::array<std::array<std::int16_t, hidden_size>, 128> black_accumulator_stack;
    std::array<accumulator_state, 128> state_stack;
    unsigned int index;

    perspective_network() {
        refresh();
    }

    void refresh() {
        white_accumulator_stack[0] = black_accumulator_stack[0] = weights.feature_bias;
        state_stack[0] = {};
        index = 0;
    }

    void push() {
        index++;
        state_stack[index] = {};
        state_stack[index].king_square = state_stack[index - 1].king_square;
    }

    void pull() {
        index--;
    }

    void set_king_squares(Square white_king, Square black_king) {
        state_stack[index].king_square = {white_king, black_king};
    }

    template<Operation operation>
    void record_change(Piece piece, Color color, Square square) {
        auto& state = state_stack[index];
        state.changes[state.change_count++] = {piece, color, square, operation};
    }

    void mark_current_computed() {
        state_stack[index].computed = true;
    }

    void materialize(const board& chessboard) {
        auto& current_state = state_stack[index];
        if (current_state.computed) {
            return;
        }

        if (index == 0) {
            refresh_accumulator<White>(chessboard, current_state.king_square[White]);
            refresh_accumulator<Black>(chessboard, current_state.king_square[Black]);
            current_state.computed = true;
            return;
        }

        const auto& parent_state = state_stack[index - 1];
        const Square white_king = current_state.king_square[White];
        const Square black_king = current_state.king_square[Black];
        const bool refresh_white = buckets[white_king] !=
                                   buckets[parent_state.king_square[White]];
        const bool refresh_black = buckets[black_king ^ 56] !=
                                   buckets[parent_state.king_square[Black] ^ 56];

        if (refresh_white) {
            refresh_accumulator<White>(chessboard, white_king);
        } else {
            white_accumulator_stack[index] = white_accumulator_stack[index - 1];
            apply_frame<White>(white_king);
        }

        if (refresh_black) {
            refresh_accumulator<Black>(chessboard, black_king);
        } else {
            black_accumulator_stack[index] = black_accumulator_stack[index - 1];
            apply_frame<Black>(black_king);
        }

        current_state.computed = true;
    }

    int get_square_index(int square, int king_square) {
        return (king_square % 8 > 3) ? square ^ 7 : square;
    }

    template<Operation operation, Color perspective>
    void update_accumulator(const Piece piece, const Color color, const Square square, int king) {
        if constexpr (perspective == White) {
            const auto feature = nnue_feature_index(buckets[king] % Buckets, color, piece,
                                                    get_square_index(square, king));
            const auto& white_weights = weights.feature_weight[feature];
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
            const auto feature = nnue_feature_index(buckets[king ^ 56] % Buckets, color ^ 1,
                                                    piece, get_square_index(square, king) ^ 56);
            const auto& black_weights = weights.feature_weight[feature];
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
        const auto white_feature = nnue_feature_index(buckets[wking] % Buckets, color, piece,
                                                      get_square_index(square, wking));
        const auto black_feature = nnue_feature_index(buckets[bking ^ 56] % Buckets, color ^ 1,
                                                      piece, get_square_index(square, bking) ^ 56);
        const auto& white_weights = weights.feature_weight[white_feature];
        const auto& black_weights = weights.feature_weight[black_feature];

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

private:
    template<Color perspective>
    void apply_frame(Square king) {
        const auto& state = state_stack[index];
        for (std::uint8_t change_index = 0; change_index < state.change_count; change_index++) {
            const feature_change& change = state.changes[change_index];
            if (change.operation == Operation::Set) {
                update_accumulator<Operation::Set, perspective>(change.piece, change.color,
                                                                change.square, king);
            } else {
                update_accumulator<Operation::Unset, perspective>(change.piece, change.color,
                                                                  change.square, king);
            }
        }
    }

    template<Color perspective>
    void refresh_accumulator(const board& chessboard, Square king) {
        auto& current_accumulator = perspective == White
                ? white_accumulator_stack[index]
                : black_accumulator_stack[index];

        current_accumulator = weights.feature_bias;
        for (Color side : {White, Black}) {
            for (Piece piece : {Pawn, Knight, Bishop, Rook, Queen, King}) {
                std::uint64_t bitboard = chessboard.get_pieces(side, piece);
                while (bitboard) {
                    const Square square = pop_lsb(bitboard);
                    update_accumulator<Operation::Set, perspective>(piece, side, square, king);
                }
            }
        }
    }

public:
#if !defined(__AVX2__) && !defined(__aarch64__) && !defined(_M_ARM64)
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
#if defined(__AVX2__)
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
#elif defined(__aarch64__) || defined(_M_ARM64)
    // NEON equivalent of the AVX2 kernel: clamp to [0, QA], take the low 16 bits
    // of clamped * weight (exact while |clamped * weight| < 2^15, i.e. |weight| <= QB),
    // then widen-multiply-accumulate by clamped into 32-bit lanes like madd does
    std::int32_t flatten(const std::int16_t* accumulator, const std::int16_t* weights) {
        constexpr std::size_t CHUNK = 8;
        constexpr std::size_t UNROLL = 4;
        const int16x8_t min = vdupq_n_s16(0);
        const int16x8_t max = vdupq_n_s16(QA);
        // independent accumulators so the vmlal chains can overlap
        int32x4_t sum_lo[UNROLL] = {};
        int32x4_t sum_hi[UNROLL] = {};
        for (std::size_t i = 0; i < hidden_size; i += UNROLL * CHUNK) {
            for (std::size_t j = 0; j < UNROLL; j++) {
                int16x8_t us_vector = vld1q_s16(accumulator + i + j * CHUNK);
                int16x8_t weights_vec = vld1q_s16(weights + i + j * CHUNK);
                int16x8_t clamped = vminq_s16(vmaxq_s16(us_vector, min), max);
                int16x8_t mul = vmulq_s16(clamped, weights_vec);
                sum_lo[j] = vmlal_s16(sum_lo[j], vget_low_s16(clamped), vget_low_s16(mul));
                sum_hi[j] = vmlal_high_s16(sum_hi[j], clamped, mul);
            }
        }
        int32x4_t sum = vdupq_n_s32(0);
        for (std::size_t j = 0; j < UNROLL; j++) {
            sum = vaddq_s32(sum, vaddq_s32(sum_lo[j], sum_hi[j]));
        }
        return vaddvq_s32(sum);
    }
#endif
#endif
};

#endif //MOTOR_NNUE_HPP
