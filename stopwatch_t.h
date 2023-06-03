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

        if(movetime > 0) {
            time_limit = movetime;
        } else if(time == -1) {
            infinite_time = true;
        } else if(increment == 0) {
            time_limit = 975 * time / movestogo / 1000;
        } else {
            time_limit = 975 * increment * time / movestogo / 1000;
        }
    }

    bool stopped() const {
        return stop;
    }

    bool should_end(uint64_t nodes = 0) {
        if (stop) {
            return true;
        } else if((nodes & 1024) && (!infinite_time)) {
            stop = elapsed() >= time_limit;
        }
        return stop;
    }

    int elapsed() {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(std::chrono::steady_clock::now() - start_time).count());
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    bool stop;
    int time_limit;
    bool infinite_time;
};

#endif //MOTOR_TIME_H
