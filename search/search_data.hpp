#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "time_keeper.hpp"

constexpr std::int16_t INF = 20'000;

class search_data {
public:
    search_data() : principal_variation_table(), timekeeper(), ply(0), nodes_searched(0) {}

    void set_timekeeper(int time, int bonus, int movestogo) {
        timekeeper.reset(time, bonus, movestogo);
    }

    bool should_end() {
        return timekeeper.should_end(nodes_searched);
    }

    void update_pv_length() {
        principal_variation_table.set_length(ply);
        nodes_searched++;
    }

    void update_pv(const chess_move move) {
        principal_variation_table.update_principal_variation(move, ply);
    }

    [[nodiscard]] std::int16_t mate_value() const {
        return -INF + ply;
    }

    [[nodiscard]] std::string get_best_move() const {
        return principal_variation_table.get_best_move().to_string();
    }

    void reduce_ply() {
        ply--;
    }

    void augment_ply() {
        ply++;
    }

private:
    std::int16_t ply;

    pv_table principal_variation_table;

    time_keeper timekeeper;


    std::uint64_t nodes_searched;
};

#endif //MOTOR_SEARCH_DATA_HPP
