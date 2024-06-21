#ifndef MOTOR_TIME_KEEPER_HPP
#define MOTOR_TIME_KEEPER_HPP

#include <chrono>
#include <cstdint>
#include <climits>
#include "../chess_board/chess_move.hpp"

struct time_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0, max_depth = 64;
};

class time_keeper {
public:
    time_keeper() : stop(false), inf_time(false), time_limit(0), optimal_time_limit(0), max_nodes(INT_MAX / 2), total_nodes(0), node_count{} {}

    void reset(int time, int increment = 0, int movestogo = 0, int move_count = 1) {
        start_time = std::chrono::steady_clock::now();
        stop = false;
        const int time_minus_threshold = time - 50;
        max_nodes = INT_MAX / 2;
        total_nodes = 0;
        inf_time = false;
        node_count = {};

        if (time == -1) {
            inf_time = true;
        }
        else if(movestogo == 0) {
            double time_divider = 40 * std::pow(1.0 + 1.5 * std::pow(double(move_count) / 40, 2), 0.5) - move_count;
            optimal_time_limit = std::clamp(0.8 * (time_minus_threshold / time_divider + increment), 10.0, std::max(50.0, time_minus_threshold / 2.0));
            time_limit = std::clamp(time_minus_threshold / sqrt(time_divider) + increment, 10.0, std::max(50.0, time_minus_threshold / 2.0));
        } else {
            time_limit = increment + 950 * time / movestogo / 1000;
            optimal_time_limit = time_limit / 4 * 3;
        }
        optimal_time_limit = std::min(optimal_time_limit, time_minus_threshold);
        time_limit = std::min(time_limit, time_minus_threshold);
    }

    [[nodiscard]] bool stopped() const {
        return stop;
    }

    bool can_end(std::uint64_t nodes, const chess_move& best_move, int depth) { // called in iterative deepening
        if (stop) {
            return true;
        }

        if (inf_time) {
            return false;
        }

        double opt_scale = 1.0;
        if (depth > 6) {
            double bm_frac = 1.0 - double(node_count[best_move.get_from()][best_move.get_to()]) / nodes;
            opt_scale = bm_frac * 2.0 + 0.5;
        }

        if (elapsed() >= std::min(optimal_time_limit * opt_scale, double(time_limit))) {
            stop = true;
        }
        return stop;
    }

    bool should_end(std::uint64_t nodes = 0) { // called in alphabeta
        if (stop) {
            return true;
        }

        if (inf_time) {
            return false;
        }

        if((nodes & 1023) == 0) {
            stop = elapsed() >= time_limit || total_nodes >= max_nodes;
        }
        return stop;
    }

    int elapsed() {
        auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count());
    }

    std::uint64_t NPS(uint64_t nodes){
        total_nodes += nodes;
        std::uint64_t elapsed_time = elapsed();

        if(elapsed_time > 0) {
            return (total_nodes / elapsed_time) * 1000;
        }

        return 0;
    }

    [[nodiscard]] std::uint64_t get_total_nodes() const {
        return total_nodes;
    }

    void stop_timer() {
        stop = true;
    }

    void update_node_count(int from, int to, int delta) {
        node_count[from][to] += delta;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    bool stop;
    bool inf_time;
    int time_limit;
    int optimal_time_limit;
    int max_nodes;
    std::uint64_t total_nodes;
    std::array<std::array<int, 64>, 64> node_count;
};

#endif //MOTOR_TIME_KEEPER_HPP
