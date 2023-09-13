#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "time_keeper.hpp"

constexpr std::int16_t INF = 20'000;

class search_data {
public:
    search_data() : principal_variation_table(), timekeeper(), ply(0), nodes_searched(0) {
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                history_moves[i][j] = 1500;
            }
        }
    }

    void set_timekeeper(int time, int bonus, int movestogo) {
        timekeeper.reset(time, bonus, movestogo);
    }

    bool should_end() {
        nodes_searched++;
        return timekeeper.should_end(nodes_searched);
    }

    bool time_is_up() {
        return timekeeper.stopped();
    }

    void update_pv_length() {
        principal_variation_table.set_length(ply);
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

    void update_killer(chess_move move) {
        killer_moves[ply][0] = killer_moves[ply][1];
        killer_moves[ply][1] = move;
    }

    chess_move get_killer(int index) {
        return killer_moves[ply][index];
    }

    void update_history(std::uint8_t from, std::uint8_t to, std::int8_t depth) {
        history_moves[from][to] += depth * depth;
    }

    std::uint16_t get_history(std::uint8_t from, std::uint8_t to) {
        return history_moves[from][to];
    }

    [[nodiscard]] std::int16_t get_ply() const {
        return ply;
    }

    chess_move previous_move;
    chess_move counter_moves[64][64] = {};
    std::int16_t eval_grandfather{};
    std::int16_t eval_father{};
private:
    std::int16_t ply;

    pv_table principal_variation_table;

    time_keeper timekeeper;

    std::uint64_t nodes_searched;
    chess_move killer_moves[MAX_DEPTH][2] = {};
    std::uint16_t history_moves[64][64];
};

#endif //MOTOR_SEARCH_DATA_HPP