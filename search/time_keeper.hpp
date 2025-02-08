#ifndef MOTOR_TIME_KEEPER_HPP
#define MOTOR_TIME_KEEPER_HPP

#include <chrono>
#include <cstdint>
#include <climits>
#include "../chess_board/chess_move.hpp"
#include "tuning_options.hpp"

struct time_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0, max_depth = 64, max_nodes = INT_MAX / 2;
};

TuningOption tm_expect_mul("tm_expect_mul", 41, 20, 70);
TuningOption tm_mul("tm_mul", 86, 40, 150);
TuningOption tm_stability_const("tm_stability_const", 137, 50, 400);
TuningOption tm_stability_mul("tm_stability_mul", 35, 10, 100);
TuningOption tm_node_const("tm_node_const", 51, 10, 400);
TuningOption tm_node_mul("tm_node_mul", 200, 50, 400);

class time_keeper {
public:
    time_keeper() : stop(false), inf_time(false), time_limit(0), optimal_time_limit(0), max_nodes(INT_MAX / 2), total_nodes(0), node_count{}, eval_correction(0) {}

    void reset(int time, int increment = 0, int movestogo = 0, int move_count = 1, std::uint64_t nodes = INT_MAX / 2) {
        start_time = std::chrono::steady_clock::now();
        stop = false;
        const int time_minus_threshold = time - 50;
        max_nodes = nodes;
        total_nodes = 0;
        inf_time = false;
        node_count = {};
        last_best_move = {};
        stability_count = 0;
        eval_correction = 0;

        if (time == -1) {
            inf_time = true;
        } else if (movestogo == 0) {
            double time_divider = tm_expect_mul.value * std::pow(1.0 + 1.5 * std::pow(double(move_count) / tm_expect_mul.value, 2), 0.5) - move_count;
            optimal_time_limit = std::clamp((tm_mul.value / 100.0) * (time_minus_threshold / time_divider + increment), 10.0, std::max(50.0, time_minus_threshold / 2.0));
            time_limit = std::clamp(time_minus_threshold / std::log(time_divider) + increment, 10.0, std::max(50.0, time_minus_threshold / 2.0));
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

        if (total_nodes >= max_nodes) {
            return true;
        }

        if (inf_time) {
            return false;
        }

        double opt_scale = 1.0;
        double stability_scale = 1.0;
        double corr_scale = 1.0;
        if (depth > 6) {
            if (last_best_move == best_move) {
                stability_count++;
            } else {
                stability_count = 0;
                last_best_move = best_move;
            }
            stability_scale = tm_stability_const.value / 100.0 - tm_stability_mul.value / 1000.0 * std::min(10, stability_count);

            double bm_frac = 1.0 - double(node_count[best_move.get_from()][best_move.get_to()]) / nodes;
            opt_scale = bm_frac * tm_node_mul.value / 100.0 + tm_node_const.value / 100.0;
            corr_scale = (eval_correction < 20) ? 0.99 : (eval_correction < 150) ? 1.0 : (eval_correction < 195) ? 1.03 : 1.07;
        }

        if (elapsed() >= std::min(optimal_time_limit * opt_scale * stability_scale * corr_scale, double(time_limit))) {
            stop = true;
        }
        return stop;
    }

    bool should_end(std::uint64_t nodes = 0) { // called in alphabeta
        if (stop || total_nodes >= max_nodes) {
            return true;
        }

        if (inf_time) {
            return false;
        }

        if((nodes & 1023) == 0) {
            stop = elapsed() >= time_limit;
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

    void update_corr_size(int correction) {
        eval_correction = correction;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    bool stop;
    bool inf_time;
    int time_limit;
    int optimal_time_limit;
    std::uint64_t max_nodes;
    std::uint64_t total_nodes;
    std::array<std::array<int, 64>, 64> node_count;
    chess_move last_best_move;
    int stability_count;
    int eval_correction;
};

#endif //MOTOR_TIME_KEEPER_HPP
