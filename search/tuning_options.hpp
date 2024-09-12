#ifndef MOTOR_TUNING_OPTIONS_HPP
#define MOTOR_TUNING_OPTIONS_HPP

#include <string>
#include <algorithm>

struct TuningOption {
    std::string name;
    int value;
    int min_value;
    int max_value;

    TuningOption(const std::string& name, int default_value, int min_value, int max_value)
            : name(name), value(default_value), min_value(min_value), max_value(max_value) {}

    std::string get_name() {
        return name;
    }

    void set_value(int new_value) {
        value = std::clamp(new_value, min_value, max_value);
    }
};

#endif //MOTOR_TUNING_OPTIONS_HPP