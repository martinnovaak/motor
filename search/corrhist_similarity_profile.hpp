#ifndef MOTOR_CORRHIST_SIMILARITY_PROFILE_HPP
#define MOTOR_CORRHIST_SIMILARITY_PROFILE_HPP

#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace corrhist_profile {

enum class Kind : std::size_t {
    Pawn,
    Threat,
    Nonpawn,
    Minor,
    Major,
    Continuation1,
    Continuation2,
    KingShelter,
    PassedPawnStructure,
    KingPressure,
    PieceSquareRegion,
    Count,
};

inline constexpr std::array<const char *, static_cast<std::size_t>(Kind::Count)> NAMES = {
    "pawn", "threat", "nonpawn", "minor", "major",
    "continuation1", "continuation2", "king_shelter",
    "passed_pawn_structure", "king_pressure", "piece_square_region",
};

struct Counters {
    std::uint64_t lookup_cold = 0;
    std::uint64_t lookup_exact = 0;
    std::uint64_t lookup_collision = 0;
    std::uint64_t lookup_unavailable = 0;
    std::uint64_t update_cold = 0;
    std::uint64_t update_exact = 0;
    std::uint64_t update_collision = 0;
    std::uint64_t update_unavailable = 0;
};

class ShadowTable {
public:
    void resize(std::size_t size) {
        owners.resize(size);
        valid.resize(size);
    }

    void lookup(std::uint64_t key, std::size_t index) {
        if (!valid[index]) counters.lookup_cold++;
        else if (owners[index] == key) counters.lookup_exact++;
        else counters.lookup_collision++;
    }

    void update(std::uint64_t key, std::size_t index) {
        if (!valid[index]) counters.update_cold++;
        else if (owners[index] == key) counters.update_exact++;
        else counters.update_collision++;
        owners[index] = key;
        valid[index] = true;
    }

    void unavailable(bool update) {
        if (update) counters.update_unavailable++;
        else counters.lookup_unavailable++;
    }

    [[nodiscard]] const Counters &statistics() const {
        return counters;
    }

private:
    std::vector<std::uint64_t> owners;
    std::vector<bool> valid;
    Counters counters;
};

inline std::atomic<std::uint32_t> next_profile_id = 0;

class Profiler {
public:
    Profiler() {
        const char *environment_prefix = std::getenv("MOTOR_CORRHIST_PROFILE_PREFIX");
        if (environment_prefix == nullptr || *environment_prefix == '\0') return;
        prefix = environment_prefix;
        enabled = true;
        id = next_profile_id.fetch_add(1, std::memory_order_relaxed);
        if (const char *rate = std::getenv("MOTOR_CORRHIST_PROFILE_SAMPLE_RATE")) {
            try {
                sample_rate = std::max<std::uint64_t>(1, std::stoull(rate));
            } catch (...) {
                sample_rate = 16;
            }
        }

        constexpr std::size_t per_color_table = 2 * 16'384;
        tables[index(Kind::Pawn)].resize(per_color_table);
        tables[index(Kind::Threat)].resize(per_color_table);
        tables[index(Kind::Nonpawn)].resize(4 * 16'384);
        tables[index(Kind::Minor)].resize(per_color_table);
        tables[index(Kind::Major)].resize(per_color_table);
        tables[index(Kind::Continuation1)].resize(7 * 64 * 7 * 64);
        tables[index(Kind::Continuation2)].resize(7 * 64 * 7 * 64);
        tables[index(Kind::KingShelter)].resize(per_color_table);
        tables[index(Kind::PassedPawnStructure)].resize(per_color_table);
        tables[index(Kind::KingPressure)].resize(per_color_table);
        tables[index(Kind::PieceSquareRegion)].resize(per_color_table);
    }

    Profiler(const Profiler &) = delete;
    Profiler &operator=(const Profiler &) = delete;

    ~Profiler() {
        if (!enabled) return;
        const std::filesystem::path path = prefix + ".h" + std::to_string(id) + ".json";
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream output(path);
        output << "{\n  \"sample_rate\": " << sample_rate << ",\n  \"tables\": {\n";
        for (std::size_t kind = 0; kind < tables.size(); kind++) {
            const Counters &value = tables[kind].statistics();
            output << "    \"" << NAMES[kind] << "\": {"
                   << "\"lookup_cold\":" << value.lookup_cold << ','
                   << "\"lookup_exact\":" << value.lookup_exact << ','
                   << "\"lookup_collision\":" << value.lookup_collision << ','
                   << "\"lookup_unavailable\":" << value.lookup_unavailable << ','
                   << "\"update_cold\":" << value.update_cold << ','
                   << "\"update_exact\":" << value.update_exact << ','
                   << "\"update_collision\":" << value.update_collision << ','
                   << "\"update_unavailable\":" << value.update_unavailable << '}';
            output << (kind + 1 == tables.size() ? "\n" : ",\n");
        }
        output << "  }\n}\n";
    }

    [[nodiscard]] bool begin_lookup() {
        if (!enabled) return false;
        return lookup_counter++ % sample_rate == 0;
    }

    [[nodiscard]] bool is_enabled() const {
        return enabled;
    }

    void lookup(Kind kind, std::uint64_t key, std::size_t table_index) {
        tables[index(kind)].lookup(key, table_index);
    }

    void update(Kind kind, std::uint64_t key, std::size_t table_index) {
        if (!enabled) return;
        tables[index(kind)].update(key, table_index);
    }

    void unavailable(Kind kind, bool update) {
        if (!enabled) return;
        tables[index(kind)].unavailable(update);
    }

private:
    static constexpr std::size_t index(Kind kind) {
        return static_cast<std::size_t>(kind);
    }

    std::array<ShadowTable, static_cast<std::size_t>(Kind::Count)> tables;
    std::string prefix;
    std::uint64_t sample_rate = 16;
    std::uint64_t lookup_counter = 0;
    std::uint32_t id = 0;
    bool enabled = false;
};

} // namespace corrhist_profile

#endif // MOTOR_CORRHIST_SIMILARITY_PROFILE_HPP
