#ifndef MOTOR_PV_TABLE_HPP
#define MOTOR_PV_TABLE_HPP

#include <cstdint>
#include <sstream>
#include "../chess_board/chess_move.hpp"

constexpr std::uint8_t MAX_DEPTH = 64;

class pv_table {
public:
    pv_table() : pv_length{}, triangular_pv_table{} {}

    void update_principal_variation(chess_move move, std::int8_t ply) {
        triangular_pv_table[ply][ply] = move;
        for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++) {
            triangular_pv_table[ply][next_ply] = triangular_pv_table[ply + 1][next_ply];
        }
        pv_length[ply] = pv_length[ply + 1];
    }

    [[nodiscard]] std::string get_pv_line(int length) const {
        std::stringstream line;
        for (int i = 0; i < std::min(int(pv_length[0]), length); i++) {
            line << triangular_pv_table[0][i].to_string() << " ";
        }
        return line.str();
    }


    [[nodiscard]] chess_move get_best_move() const {
        return triangular_pv_table[0][0];
    }

    void set_length(std::uint8_t length) {
        pv_length[length] = length;
    }

private:
    std::uint8_t pv_length[96];
    chess_move triangular_pv_table[96][96];
};

#endif //MOTOR_PV_TABLE_HPP
