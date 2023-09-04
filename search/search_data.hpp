#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "time_keeper.hpp"
#include "../chess_board/chess_move.hpp"

class search_data {
public:
    search_data() : principal_variation_table(), timekeeper(), ply(0), nodes_searched(0) {}

    void set_timekeeper(int time, int bonus, int movestogo) {
        timekeeper.reset(time, bonus, movestogo);
    }
private:
    pv_table principal_variation_table;

    time_keeper timekeeper;

    std::int8_t ply;
    std::uint64_t nodes_searched;
};

#endif //MOTOR_SEARCH_DATA_HPP
