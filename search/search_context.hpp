#ifndef MOTOR_SEARCH_CONTEXT_HPP
#define MOTOR_SEARCH_CONTEXT_HPP

#include <atomic>
#include <cstdint>

#include "tables/transposition_table.hpp"

struct shared_search_state {
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> nodes{0};

    void new_search() {
        stop.store(false, std::memory_order_relaxed);
        nodes.store(0, std::memory_order_relaxed);
    }
};

class search_context {
public:
    explicit search_context(std::uint64_t hash_size = 32ULL * 1024 * 1024)
        : table(hash_size) {}

    void new_search() {
        shared.new_search();
    }

    void stop() {
        shared.stop.store(true, std::memory_order_relaxed);
    }

    transposition_table<TT_cluster> table;
    shared_search_state shared;
};

#endif // MOTOR_SEARCH_CONTEXT_HPP
