#ifndef MOTOR_CORRHIST_TRACE_HPP
#define MOTOR_CORRHIST_TRACE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "correction_history.hpp"

namespace corrhist_trace {

constexpr std::uint16_t VERSION = 2;
constexpr std::uint16_t HEADER_SIZE = 64;
constexpr std::uint16_t RECORD_SIZE = 32;
constexpr std::size_t BUFFER_SIZE = 1U << 20;

constexpr std::uint8_t FLAG_PV = 1U << 0;
constexpr std::uint8_t FLAG_ROOT = 1U << 1;
constexpr std::uint8_t FLAG_MAIN_THREAD = 1U << 2;
constexpr std::uint8_t FLAG_QUIET_BEST_MOVE = 1U << 3;
constexpr std::uint8_t FLAG_BLACK_TO_MOVE = 1U << 4;

class Writer {
public:
    explicit Writer(const std::filesystem::path &path,
                    std::uint32_t requested_sample_rate = 1)
            : sample_rate(std::max<std::uint32_t>(1, requested_sample_rate)) {
        buffer.reserve(BUFFER_SIZE + RECORD_SIZE);
        if (std::filesystem::exists(path)) {
            std::cerr << "Corrhist trace already exists: " << path << '\n';
            return;
        }

        stream.open(path, std::ios::binary | std::ios::in | std::ios::out
                          | std::ios::trunc);
        if (!stream) {
            std::cerr << "Unable to create corrhist trace: " << path << '\n';
            return;
        }
        write_header();
        stream.seekp(0, std::ios::end);
        enabled = static_cast<bool>(stream);
    }

    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    ~Writer() {
        flush();
    }

    void begin_search() {
        search_id++;
    }

    void set_game_id(std::uint32_t id) {
        game_id = id;
    }

    [[nodiscard]] bool is_enabled() const {
        return enabled;
    }

    void append(int raw_eval,
                int search_score,
                const CorrectionHistoryComponents &components,
                std::uint8_t material_phase,
                int depth,
                std::uint8_t bound,
                std::uint8_t flags) {
        if (!enabled || !should_sample(raw_eval, search_score, components,
                                       material_phase, depth)) {
            return;
        }

        append_i16(raw_eval);
        append_i16(search_score);
        append_i16(components.pawn);
        append_i16(components.threat);
        append_i16(components.nonpawn);
        append_i16(components.minor);
        append_i16(components.major);
        append_i16(components.continuation1);
        append_i16(components.continuation2);
        buffer.push_back(material_phase);
        buffer.push_back(static_cast<std::uint8_t>(std::clamp(depth, 0, 255)));
        append_u32(buffer, search_id);
        append_u32(buffer, game_id);
        buffer.push_back(bound);
        buffer.push_back(flags);
        append_u16(buffer, 0);
        record_count++;

        if (buffer.size() >= BUFFER_SIZE) {
            flush();
        }
    }

    void flush() {
        if (!enabled) {
            return;
        }
        if (!buffer.empty()) {
            stream.seekp(0, std::ios::end);
            stream.write(reinterpret_cast<const char *>(buffer.data()),
                         static_cast<std::streamsize>(buffer.size()));
            buffer.clear();
        }
        stream.seekp(0);
        write_header();
        stream.seekp(0, std::ios::end);
        stream.flush();
    }

private:
    static void append_u16(std::vector<std::uint8_t> &output,
                           std::uint16_t value) {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    static void append_u32(std::vector<std::uint8_t> &output,
                           std::uint32_t value) {
        for (int byte = 0; byte < 4; byte++) {
            output.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
        }
    }

    static void append_u64(std::vector<std::uint8_t> &output,
                           std::uint64_t value) {
        for (int byte = 0; byte < 8; byte++) {
            output.push_back(static_cast<std::uint8_t>(value >> (byte * 8)));
        }
    }

    void append_i16(int value) {
        const auto clamped = static_cast<std::int16_t>(
                std::clamp(value,
                           static_cast<int>(std::numeric_limits<std::int16_t>::min()),
                           static_cast<int>(std::numeric_limits<std::int16_t>::max())));
        append_u16(buffer, static_cast<std::uint16_t>(clamped));
    }

    static std::uint64_t mix(std::uint64_t value) {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    [[nodiscard]] bool should_sample(
            int raw_eval,
            int search_score,
            const CorrectionHistoryComponents &components,
            std::uint8_t material_phase,
            int depth) const {
        if (sample_rate == 1) {
            return true;
        }
        std::uint64_t key = static_cast<std::uint64_t>(game_id) << 32 | search_id;
        key ^= static_cast<std::uint64_t>(static_cast<std::uint16_t>(raw_eval));
        key ^= static_cast<std::uint64_t>(
                static_cast<std::uint16_t>(search_score)) << 16;
        key ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(depth)) << 48;
        key ^= static_cast<std::uint64_t>(material_phase) << 56;
        key ^= static_cast<std::uint64_t>(
                static_cast<std::uint16_t>(components.pawn)) << 1;
        key ^= static_cast<std::uint64_t>(
                static_cast<std::uint16_t>(components.nonpawn)) << 17;
        key ^= static_cast<std::uint64_t>(
                static_cast<std::uint16_t>(components.major)) << 33;
        return mix(key) % sample_rate == 0;
    }

    void write_header() {
        std::vector<std::uint8_t> header;
        header.reserve(HEADER_SIZE);
        constexpr std::array<std::uint8_t, 8> magic = {
            'M', 'C', 'R', 'H', 'I', 'S', 'T', 0,
        };
        header.insert(header.end(), magic.begin(), magic.end());
        append_u16(header, VERSION);
        append_u16(header, HEADER_SIZE);
        append_u16(header, RECORD_SIZE);
        append_u16(header, 0);
        append_u64(header, record_count);
        append_u32(header, sample_rate);
        header.resize(HEADER_SIZE, 0);
        stream.write(reinterpret_cast<const char *>(header.data()),
                     static_cast<std::streamsize>(header.size()));
    }

    std::fstream stream;
    std::vector<std::uint8_t> buffer;
    std::uint64_t record_count = 0;
    std::uint32_t search_id = 0;
    std::uint32_t game_id = 0;
    std::uint32_t sample_rate = 1;
    bool enabled = false;
};

inline thread_local Writer *active_writer = nullptr;

inline void activate(Writer *writer) {
    active_writer = writer;
}

inline void begin_search() {
    if (active_writer != nullptr) {
        active_writer->begin_search();
    }
}

inline void set_game_id(std::uint32_t game_id) {
    if (active_writer != nullptr) {
        active_writer->set_game_id(game_id);
    }
}

inline void append(int raw_eval,
                   int search_score,
                   const CorrectionHistoryComponents &components,
                   std::uint8_t material_phase,
                   int depth,
                   std::uint8_t bound,
                   std::uint8_t flags) {
    if (active_writer != nullptr) {
        active_writer->append(raw_eval, search_score, components, material_phase,
                              depth, bound, flags);
    }
}

} // namespace corrhist_trace

#endif // MOTOR_CORRHIST_TRACE_HPP
