#ifndef MOTOR_TIME_H
#define MOTOR_TIME_H

#include <chrono>

class stopwatch_t {
public:
    stopwatch_t() : infinite_time(false), stop(false){}

    void reset(int time = -1, int increment = 0, int movestogo = -1, int movetime = -1) {
        start_time = std::chrono::steady_clock::now();
        stop = false;
        infinite_time = false;
        max_nodes = INT_MAX / 2;

        if(movetime > 0) {
            time_limit = movetime;
        } else if(time == -1) {
            infinite_time = true;
        } else {
            if(movestogo == 0) {
                time_limit = increment + time / 50;
            } else {
                time_limit = increment + 975 * time / movestogo / 1000;
            }
        }
    }

    bool stopped() const {
        return stop;
    }

    bool should_end(uint64_t nodes = 0) {
        if (stop) {
            return true;
        } else if((nodes & 1024) && (!infinite_time)) {
            stop = elapsed() >= time_limit || nodes >= max_nodes;
        }
        return stop;
    }

    int elapsed() {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(std::chrono::steady_clock::now() - start_time).count());
    }

    uint64_t NPS(uint64_t nodes){
        uint64_t elapsed_time = elapsed();
        if(elapsed_time > 0) {
            return (nodes / elapsed_time) * 1000;
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
    bool infinite_time;
    int max_nodes;
};

#endif //MOTOR_TIME_H
