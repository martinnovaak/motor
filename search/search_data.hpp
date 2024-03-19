#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "time_keeper.hpp"
#include "tables/butterfly_table.hpp"

constexpr std::int16_t INF = 20'000;

butterfly_table<std::int32_t> history_table(0);

class search_data {
public:
    search_data() : ply(0), principal_variation_table(), timekeeper(), nodes_searched(0)  {}

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

    std::string get_pv(int length) {
        return principal_variation_table.get_pv_line(length);
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

    void update_history(std::uint8_t from, std::uint8_t to, std::int8_t depth){
        history_table.increase_value(from, to, depth * depth);
    }

    void reduce_history(std::uint8_t from, std::uint8_t to, std::int8_t depth){
        history_table.reduce_value(from, to, depth * depth);
    }

    std::int16_t get_history(std::uint8_t from, std::uint8_t to) {
        return history_table.get_value(from, to);
    }

    [[nodiscard]] std::int16_t get_ply() const {
        return ply;
    }

    std::uint64_t nodes() {
        return timekeeper.get_total_nodes();
    }

    void reset_nodes() {
        nodes_searched = 0;
    }

    std::uint64_t nps() {
        return timekeeper.NPS(nodes_searched);
    }

    int improving[96] = {};
    chess_move counter_moves[64][64] = {};
    std::int16_t singular_move = {};
    int stack_eval = {};
private:
    std::int16_t ply;

    pv_table principal_variation_table;

    time_keeper timekeeper;

    std::uint64_t nodes_searched;
    chess_move killer_moves[MAX_DEPTH][2] = {};
};

#endif //MOTOR_SEARCH_DATA_HPP