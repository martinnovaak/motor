#ifndef MOTOR_SEARCH_DATA_HPP
#define MOTOR_SEARCH_DATA_HPP

#include <cstdint>

#include "pv_table.hpp"
#include "time_keeper.hpp"
#include "tables/transposition_table.hpp"

constexpr std::int16_t INF = 20'000;

enum class NodeType : std::uint8_t {
    Root, PV, Non_PV, Null
};

transposition_table<TT_cluster> tt(32 * 1024 * 1024);

class History;

struct history_move {
    Piece piece_type = Piece::Null_Piece;
    Square from = Square::A1;
    Square to = Square::A1;
};

class search_data {
public:
    search_data() : ply(0), principal_variation_table(), timekeeper(), nodes_searched(0)  {}

    void set_timekeeper(int time, int bonus, int movestogo, int move_count, int max_nodes) {
        timekeeper.reset(time, bonus, movestogo, move_count, max_nodes);
    }

    bool should_end() {
        if ((nodes_searched & 1023) == 0) {
            flush_nodes();
        }
        return timekeeper.should_end(nodes_searched, main_thread);
    }

    bool time_stopped() {
        return timekeeper.stopped();
    }

    bool time_is_up(int depth) {
        if (!main_thread) {
            return timekeeper.stopped();
        }
        return timekeeper.can_end(principal_variation_table.get_best_move(), depth);
    }

    // push locally counted nodes into the shared counter
    void flush_nodes() {
        shared_state.nodes.fetch_add(nodes_searched - flushed_nodes, std::memory_order_relaxed);
        flushed_nodes = nodes_searched;
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
        nodes_searched++;
        ply++;
    }

    void update_killer(chess_move move) {
        killer_moves[ply] = move;
    }

    void reset_killers() {
        killer_moves[ply + 2] = {};
    }

    chess_move get_killer() {
        return killer_moves[ply];
    }


    [[nodiscard]] std::int16_t get_ply() const {
        return ply;
    }

    std::uint64_t nodes() {
        flush_nodes();
        return shared_state.nodes.load(std::memory_order_relaxed);
    }

    std::uint64_t searched_nodes() {
        return this->nodes_searched;
    }

    void reset_nodes() {
        flush_nodes();
        nodes_searched = 0;
        flushed_nodes = 0;
    }

    [[nodiscard]] std::uint64_t get_nodes() const {
        return nodes_searched;
    }

    void update_node_count(int from, int to, std::uint64_t node_count) {
        timekeeper.update_node_count(from, to, nodes_searched - node_count);
    }

    std::uint64_t nps() {
        return timekeeper.NPS(nodes_searched);
    }

    History* history = nullptr;
    bool main_thread = true;

    int improving[96] = {};

    history_move prev_moves[96];

    std::uint32_t singular_move[96] = {};
    int stack_eval = {};
    std::string best_move = {};
private:
    std::int16_t ply;

    pv_table principal_variation_table;

    time_keeper timekeeper;

    std::uint64_t nodes_searched;
    std::uint64_t flushed_nodes = 0;
    chess_move killer_moves[96] = {};
};

#endif //MOTOR_SEARCH_DATA_HPP