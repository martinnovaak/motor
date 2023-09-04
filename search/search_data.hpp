#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "../chess_board/chess_move.hpp"

class search_data {
public:
    search_data() : principal_variation_table(), ply(0), nodes_searched(0) {}
private:
    pv_table principal_variation_table;

    std::int8_t ply = 0;
    std::uint64_t nodes_searched = 0;
};

#endif //MOTOR_SEARCH_DATA_HPP
