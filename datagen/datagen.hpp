#ifndef MOTOR_DATAGEN_HPP
#define MOTOR_DATAGEN_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <pthread.h>
#endif

#include "../dataformat/dataformat.hpp"
#include "../executioner/makemove.hpp"
#include "../move_generation/move_generator.hpp"
#include "../search/move_ordering/see.hpp"
#include "../search/search.hpp"

namespace motor::datagen {

constexpr std::size_t MIN_TRAINING_PLY = 16;
constexpr int MIN_TRAINING_PIECES = 4;
constexpr int MAX_TRAINING_EVAL = 20'000;
constexpr std::size_t RANDOM_OPENING_PLIES = 8;
constexpr std::size_t RANDOM_MOVE_ATTEMPTS = 8;
constexpr int RANDOM_SEE_THRESHOLD = -1'000;
constexpr std::uint64_t RANDOM_OPENING_FILTER_NODES = 50'000;
constexpr int RANDOM_OPENING_MAX_EVAL = 1'000;
constexpr std::size_t MAX_RANDOM_OPENING_ATTEMPTS = 1'024;

struct Options {
    std::filesystem::path openings;
    std::filesystem::path output;
    bool random_openings = false;
    std::size_t threads = std::max(1U, std::thread::hardware_concurrency());
    std::uint64_t nodes = 25'000;
    std::size_t games = 0;
    std::size_t max_plies = 256;
    std::uint64_t seed = 1;
};

struct SearchResult {
    chess_move best_move{};
    std::int16_t score = 0;
};

class Player {
public:
    Player() : context(4ULL * 1024 * 1024), history(std::make_unique<History>()) {}

    void new_game() {
        context.table.clear();
    }

    SearchResult search(position& pos, std::uint64_t nodes) {
        context.new_search();
        search_data data(context);
        data.history = history.get();
        data.main_thread = true;
        data.set_timekeeper(-1, 0, 0, pos.chessboard.move_count(), nodes);

        SearchResult completed;
        std::int16_t score = 0;
        for (int depth = 1; depth <= MAX_DEPTH; depth++) {
            if (depth > 1 && data.time_is_up(depth)) {
                break;
            }

            if (depth < asp_depth) {
                score = alpha_beta_for_side(pos, data, depth);
            } else {
                score = aspiration_for_side(pos, data, score, depth);
            }

            if (depth > 1 && data.time_stopped()) {
                break;
            }

            completed = {data.get_best_move_object(), score};
            // Keep the time manager's completed-iteration node total in sync;
            // it is also used by the best-move stability calculation.
            static_cast<void>(data.nps());
            data.reset_nodes();
        }
        return completed;
    }

private:
    static std::int16_t alpha_beta_for_side(position& pos, search_data& data, int depth) {
        if (pos.chessboard.get_side() == White) {
            return alpha_beta<White, NodeType::Root>(
                    pos, data, -10'000, 10'000, static_cast<std::int8_t>(depth), false);
        }
        return alpha_beta<Black, NodeType::Root>(
                pos, data, -10'000, 10'000, static_cast<std::int8_t>(depth), false);
    }

    static std::int16_t aspiration_for_side(position& pos, search_data& data,
                                             std::int16_t score, int depth) {
        if (pos.chessboard.get_side() == White) {
            return aspiration_window<White>(pos, data, score, depth);
        }
        return aspiration_window<Black>(pos, data, score, depth);
    }

    search_context context;
    std::unique_ptr<History> history;
};

struct PendingRecord {
    dataformat::PositionRecord record;
    bool black_to_move = false;
};

inline dataformat::Bitboards bitboards_for(const board& chessboard) {
    dataformat::Bitboards result{};
    result[0] = chessboard.get_side_occupancy<White>();
    result[1] = chessboard.get_side_occupancy<Black>();
    for (int piece = Pawn; piece <= King; piece++) {
        result[2 + piece] = chessboard.get_pieces(White, piece)
                          | chessboard.get_pieces(Black, piece);
    }
    return result;
}

inline dataformat::PositionState state_for(const board& chessboard) {
    const Square en_passant = chessboard.enpassant_square();
    return {
        chessboard.castling_rights(),
        en_passant == Null_Square
                ? dataformat::NO_EN_PASSANT
                : static_cast<std::uint8_t>(en_passant % 8),
        chessboard.halfmove_clock(),
    };
}

inline std::optional<dataformat::Result> terminal_result(position& pos) {
    board& chessboard = pos.chessboard;
    move_list moves;
    if (chessboard.get_side() == White) {
        generate_all_moves<White>(chessboard, moves);
    } else {
        generate_all_moves<Black>(chessboard, moves);
    }

    if (moves.size() == 0) {
        if (!chessboard.in_check()) {
            return dataformat::Result::Draw;
        }
        return chessboard.get_side() == White
                ? dataformat::Result::Loss
                : dataformat::Result::Win;
    }

    if (chessboard.is_draw(0)) {
        return dataformat::Result::Draw;
    }

    const auto heavy_or_pawns = chessboard.get_pieces(White, Pawn)
                              | chessboard.get_pieces(Black, Pawn)
                              | chessboard.get_pieces(White, Rook)
                              | chessboard.get_pieces(Black, Rook)
                              | chessboard.get_pieces(White, Queen)
                              | chessboard.get_pieces(Black, Queen);
    const auto minors = chessboard.get_pieces(White, Knight)
                      | chessboard.get_pieces(Black, Knight)
                      | chessboard.get_pieces(White, Bishop)
                      | chessboard.get_pieces(Black, Bishop);
    if (heavy_or_pawns == 0 && popcount(minors) <= 1) {
        return dataformat::Result::Draw;
    }
    return std::nullopt;
}

inline void play_move(position& pos, chess_move move) {
    if (pos.chessboard.get_side() == White) {
        make_move<White>(pos, move);
    } else {
        make_move<Black>(pos, move);
    }
}

inline std::uint64_t splitmix64(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

inline bool random_move_passes_see(board& chessboard, chess_move move) {
    return chessboard.get_side() == White
            ? see<White>(chessboard, move, RANDOM_SEE_THRESHOLD)
            : see<Black>(chessboard, move, RANDOM_SEE_THRESHOLD);
}

inline bool play_random_move(position& pos, std::mt19937_64& random) {
    board& chessboard = pos.chessboard;
    move_list moves;
    if (chessboard.get_side() == White) {
        generate_all_moves<White>(chessboard, moves);
    } else {
        generate_all_moves<Black>(chessboard, moves);
    }
    if (moves.size() == 0) {
        return false;
    }

    std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
    chess_move selected = *(moves.begin() + pick(random));
    for (std::size_t attempt = 0; attempt < RANDOM_MOVE_ATTEMPTS; attempt++) {
        const chess_move candidate = *(moves.begin() + pick(random));
        selected = candidate;
        if (random_move_passes_see(chessboard, candidate)) {
            break;
        }
    }
    play_move(pos, selected);
    return true;
}

inline std::unique_ptr<position> random_starting_position(
        std::uint64_t seed, std::size_t game, std::size_t attempt) {
    auto pos = std::make_unique<position>();
    set_position(*pos);
    std::mt19937_64 random(splitmix64(seed ^ splitmix64(game)
                                     ^ splitmix64(attempt)));
    for (std::size_t ply = 0; ply < RANDOM_OPENING_PLIES; ply++) {
        if (!play_random_move(*pos, random)) {
            return nullptr;
        }
    }
    if (terminal_result(*pos)) {
        return nullptr;
    }
    set_position(*pos);
    return pos;
}

inline std::size_t initial_ply_from_fen(const std::string& fen) {
    std::istringstream input(fen);
    std::string placement;
    std::string side;
    std::string castling;
    std::string en_passant;
    std::uint64_t halfmove = 0;
    std::uint64_t fullmove = 0;
    if (!(input >> placement >> side >> castling >> en_passant >> halfmove >> fullmove)
        || (side != "w" && side != "b") || fullmove == 0) {
        throw std::runtime_error("invalid FEN move counters: " + fen);
    }
    if (fullmove > (std::numeric_limits<std::size_t>::max() - 1) / 2 + 1) {
        throw std::runtime_error("FEN fullmove counter is too large: " + fen);
    }
    return static_cast<std::size_t>((fullmove - 1) * 2)
         + static_cast<std::size_t>(side == "b");
}

inline bool should_store_position(board& chessboard, chess_move best_move,
                                  std::int16_t score, std::size_t absolute_ply) {
    if (absolute_ply < MIN_TRAINING_PLY
        || popcount(chessboard.get_occupancy()) < MIN_TRAINING_PIECES
        || std::abs(static_cast<int>(score)) >= MAX_TRAINING_EVAL
        || chessboard.in_check()) {
        return false;
    }

    const MoveType move_type = best_move.get_move_type();
    return move_type != PROMOTION
        && move_type != EN_PASSANT
        && !chessboard.is_capture(best_move);
}

inline std::pair<std::vector<PendingRecord>, dataformat::Result>
play_game(std::unique_ptr<position> pos, std::size_t initial_ply,
          const Options& options, Player& white, Player& black) {
    white.new_game();
    black.new_game();

    std::vector<PendingRecord> records;
    records.reserve(std::min<std::size_t>(options.max_plies, 256));
    int decisive_count = 0;
    int decisive_sign = 0;
    int draw_count = 0;
    for (std::size_t ply = 0; ply < options.max_plies; ply++) {
        if (const auto result = terminal_result(*pos)) {
            return {std::move(records), *result};
        }

        const bool black_to_move = pos->chessboard.get_side() == Black;
        Player& player = black_to_move ? black : white;
        const SearchResult search = player.search(*pos, options.nodes);
        if (search.best_move.get_value() == 0) {
            throw std::runtime_error("search returned a null best move");
        }

        if (should_store_position(pos->chessboard, search.best_move, search.score,
                                  initial_ply + ply)) {
            records.push_back({
                    dataformat::pack(bitboards_for(pos->chessboard), black_to_move,
                                     search.score, search.best_move.get_value(),
                                     state_for(pos->chessboard)),
                    black_to_move,
            });
        }

        const int white_score = black_to_move ? -search.score : search.score;
        const int sign = (white_score > 0) - (white_score < 0);
        if (std::abs(white_score) >= 2500) {
            if (sign == decisive_sign) {
                decisive_count++;
            } else {
                decisive_sign = sign;
                decisive_count = 1;
            }
        } else {
            decisive_count = 0;
            decisive_sign = 0;
        }

        if (std::abs(white_score) <= 4) {
            draw_count++;
        } else {
            draw_count = 0;
        }

        if (decisive_count >= 4 || std::abs(white_score) >= 19'000) {
            return {std::move(records), decisive_sign > 0
                    ? dataformat::Result::Win
                    : dataformat::Result::Loss};
        }
        if (draw_count >= 12) {
            return {std::move(records), dataformat::Result::Draw};
        }

        play_move(*pos, search.best_move);
        set_position(*pos);
    }
    return {std::move(records), dataformat::Result::Draw};
}

inline std::pair<std::vector<PendingRecord>, dataformat::Result>
play_game(const std::string& fen, const Options& options, Player& white, Player& black) {
    auto pos = std::make_unique<position>();
    pos->chessboard.fen_to_board(fen);
    set_position(*pos);
    return play_game(std::move(pos), initial_ply_from_fen(fen), options, white, black);
}

inline bool decimal(const std::string& value) {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

inline std::string fen_from_epd(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> fields;
    for (std::string field; input >> field;) {
        fields.push_back(std::move(field));
    }
    if (fields.size() < 4) {
        throw std::runtime_error("invalid EPD line: " + line);
    }

    std::string fen = fields[0] + " " + fields[1] + " " + fields[2] + " " + fields[3];
    if (fields.size() >= 6 && decimal(fields[4]) && decimal(fields[5])) {
        fen += " " + fields[4] + " " + fields[5];
    } else {
        fen += " 0 1";
    }
    return fen;
}

inline std::vector<std::string> read_openings(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open EPD: " + path.string());
    }

    std::vector<std::string> openings;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        openings.push_back(fen_from_epd(line.substr(first)));
    }
    if (openings.empty()) {
        throw std::runtime_error("EPD contains no positions: " + path.string());
    }
    return openings;
}

class WorkerThread {
public:
    explicit WorkerThread(std::function<void()> function) {
#if defined(_WIN32)
        thread = std::thread(std::move(function));
#else
        auto start = std::make_unique<std::function<void()>>(std::move(function));
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, 8 * 1024 * 1024);
        const int error = pthread_create(&thread, &attributes, &WorkerThread::entry, start.get());
        pthread_attr_destroy(&attributes);
        if (error != 0) {
            throw std::runtime_error("failed to create datagen worker thread");
        }
        start.release();
#endif
    }

    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    ~WorkerThread() {
        join();
    }

    void join() {
        if (joined) {
            return;
        }
#if defined(_WIN32)
        thread.join();
#else
        pthread_join(thread, nullptr);
#endif
        joined = true;
    }

private:
#if defined(_WIN32)
    std::thread thread;
#else
    static void* entry(void* argument) {
        std::unique_ptr<std::function<void()>> function(
                static_cast<std::function<void()>*>(argument));
        (*function)();
        return nullptr;
    }
    pthread_t thread{};
#endif
    bool joined = false;
};

inline void run(const Options& options) {
    if (options.threads == 0 || options.nodes == 0 || options.max_plies == 0) {
        throw std::runtime_error("threads, nodes and max plies must be positive");
    }
    if (std::filesystem::exists(options.output)) {
        throw std::runtime_error("output already exists: " + options.output.string());
    }

    if (options.random_openings && options.games == 0) {
        throw std::runtime_error("random opening mode requires a game count");
    }
    const std::vector<std::string> openings = options.random_openings
            ? std::vector<std::string>{}
            : read_openings(options.openings);
    const std::size_t game_limit = options.random_openings
            ? options.games
            : (options.games == 0 ? openings.size() : options.games);
    const auto created = static_cast<std::uint64_t>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    dataformat::Writer writer(options.output, options.seed, options.nodes, created);
    std::mutex writer_mutex;
    std::mutex error_mutex;
    std::exception_ptr error;
    std::atomic<bool> stop = false;
    std::atomic<std::size_t> next_game = 0;
    std::atomic<std::size_t> completed = 0;
    std::atomic<std::uint64_t> positions = 0;
    std::atomic<std::uint64_t> rejected_openings = 0;

    auto worker = [&] {
        try {
            Player white;
            Player black;
            while (!stop.load(std::memory_order_relaxed)) {
                const std::size_t game = next_game.fetch_add(1, std::memory_order_relaxed);
                if (game >= game_limit) {
                    return;
                }

                std::vector<PendingRecord> records;
                dataformat::Result white_result;
                if (options.random_openings) {
                    bool accepted = false;
                    for (std::size_t attempt = 0;
                         attempt < MAX_RANDOM_OPENING_ATTEMPTS; attempt++) {
                        auto pos = random_starting_position(options.seed, game, attempt);
                        if (!pos) {
                            rejected_openings.fetch_add(1, std::memory_order_relaxed);
                            continue;
                        }

                        white.new_game();
                        black.new_game();
                        Player& player = pos->chessboard.get_side() == White ? white : black;
                        const SearchResult opening = player.search(
                                *pos, RANDOM_OPENING_FILTER_NODES);
                        if (std::abs(static_cast<int>(opening.score))
                            > RANDOM_OPENING_MAX_EVAL) {
                            rejected_openings.fetch_add(1, std::memory_order_relaxed);
                            continue;
                        }

                        auto generated = play_game(std::move(pos), MIN_TRAINING_PLY,
                                                   options, white, black);
                        records = std::move(generated.first);
                        white_result = generated.second;
                        accepted = true;
                        break;
                    }
                    if (!accepted) {
                        throw std::runtime_error(
                                "failed to generate a balanced random opening");
                    }
                } else {
                    auto generated = play_game(
                            openings[game % openings.size()], options, white, black);
                    records = std::move(generated.first);
                    white_result = generated.second;
                }
                for (PendingRecord& pending : records) {
                    dataformat::set_game_result(
                            pending.record, white_result, pending.black_to_move);
                }

                {
                    std::lock_guard<std::mutex> lock(writer_mutex);
                    for (const PendingRecord& pending : records) {
                        writer.write(pending.record);
                    }
                    writer.finish_game();
                }

                positions.fetch_add(records.size(), std::memory_order_relaxed);
                const std::size_t done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
                if (done % 100 == 0 || done == game_limit) {
                    std::lock_guard<std::mutex> lock(writer_mutex);
                    std::cerr << "datagen: " << done << "/" << game_limit
                              << " games, " << positions.load(std::memory_order_relaxed)
                              << " positions";
                    if (options.random_openings) {
                        std::cerr << ", "
                                  << rejected_openings.load(std::memory_order_relaxed)
                                  << " rejected openings";
                    }
                    std::cerr << '\n';
                }
            }
        } catch (...) {
            stop.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(error_mutex);
            if (!error) {
                error = std::current_exception();
            }
        }
    };

    const std::size_t worker_count = std::min(options.threads, game_limit);
    std::vector<std::unique_ptr<WorkerThread>> workers;
    workers.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; i++) {
        workers.push_back(std::make_unique<WorkerThread>(worker));
    }
    for (auto& thread : workers) {
        thread->join();
    }

    if (error) {
        writer.abort();
        std::rethrow_exception(error);
    }
    writer.close();
}

} // namespace motor::datagen

#endif // MOTOR_DATAGEN_HPP
