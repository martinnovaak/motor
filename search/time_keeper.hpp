#ifndef MOTOR_TIME_KEEPER_HPP
#define MOTOR_TIME_KEEPER_HPP

#include <chrono>
#include <cstdint>

struct time_info {
    int wtime = -1, btime = -1, winc = 0, binc = 0, movestogo = 0;
};

class time_keeper {
public:
    time_keeper() : stop(false), total_nodes(0), time_limit(0), optimal_time_limit(0), max_nodes(INT_MAX / 2) {}

    void reset(int time, int increment = 0, int movestogo = 0) {
        start_time = std::chrono::steady_clock::now();
        stop = false;
        const int time_minus_threshold = time - 50;
        max_nodes = INT_MAX / 2;
        total_nodes = 0;

        if(movestogo == 0) {
            time_limit = std::min(time_minus_threshold / 20 + increment / 2, time);
            optimal_time_limit = time_minus_threshold / 40 + increment / 2;
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

    bool can_end() { // called in iterative deepening
        if (stop) {
            return true;
        } else if (elapsed() >= optimal_time_limit) {
            stop = true;
        }
        return stop;
    }

    bool should_end(std::uint64_t nodes = 0) { // called in alphabeta
        if (stop) {
            return true;
        } else if((nodes & 1023) == 0) {
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
        } else {
            return 0;
        }
    }

    void stop_timer() {
        stop = true;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    bool stop;
    int time_limit;
    int optimal_time_limit;
    int max_nodes;
    std::uint64_t total_nodes;
};

#endif //MOTOR_TIME_KEEPER_HPP