#ifndef MOTOR_THREAD_POOL_HPP
#define MOTOR_THREAD_POOL_HPP

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "search.hpp"

#if defined(_WIN32)
class native_thread {
public:
    native_thread(void* (*entry)(void*), void* arg) : handle(entry, arg) {}
    void join() { handle.join(); }
private:
    std::thread handle;
};
#else
#include <pthread.h>

class native_thread {
public:
    native_thread(void* (*entry)(void*), void* arg) {
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, search_stack_size);
        pthread_create(&handle, &attributes, entry, arg);
        pthread_attr_destroy(&attributes);
    }

    void join() { pthread_join(handle, nullptr); }

private:
    static constexpr std::size_t search_stack_size = 8 * 1024 * 1024;
    pthread_t handle{};
};
#endif

// persistent per-thread search state: each thread searches its own copy of
// the root position and owns its history tables across searches
struct search_thread_context {
    position pos;
    std::unique_ptr<History> history = std::make_unique<History>();
};

class thread_pool {
public:
    thread_pool() = default;

    ~thread_pool() {
        shutdown();
    }

    void resize(std::size_t thread_count) {
        shutdown();
        quit = false;
        generation = 0;
        contexts.clear();
        threads.clear();
        for (std::size_t i = 0; i < thread_count; i++) {
            contexts.push_back(std::make_unique<search_thread_context>());
        }
        for (std::size_t i = 0; i < thread_count; i++) {
            threads.push_back(std::make_unique<native_thread>(&thread_pool::worker_entry, new worker_start{this, i}));
        }
    }

    std::size_t size() const {
        return threads.size();
    }

    void start_search(const position& root, const time_info& info) {
        if (threads.empty()) {
            resize(1);
        }
        wait_until_finished();
        shared_state.new_search();
        root_info = info;
        for (auto& ctx : contexts) {
            ctx->pos = root;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            active_workers = threads.size();
            generation++;
        }
        cv_start.notify_all();
    }

    void stop_searching() {
        shared_state.stop.store(true, std::memory_order_relaxed);
    }

    void wait_until_finished() {
        std::unique_lock<std::mutex> lock(mutex);
        cv_done.wait(lock, [this] { return active_workers == 0; });
    }

    bool is_searching() {
        std::lock_guard<std::mutex> lock(mutex);
        return active_workers > 0;
    }

    // only call while idle
    void clear_histories() {
        for (auto& ctx : contexts) {
            ctx->history->clear();
        }
    }

    void shutdown() {
        stop_searching();
        {
            std::lock_guard<std::mutex> lock(mutex);
            quit = true;
        }
        cv_start.notify_all();
        for (auto& thread : threads) {
            thread->join();
        }
        threads.clear();
    }

private:
    struct worker_start {
        thread_pool* pool;
        std::size_t id;
    };

    static void* worker_entry(void* arg) {
        std::unique_ptr<worker_start> start(static_cast<worker_start*>(arg));
        start->pool->worker_loop(start->id);
        return nullptr;
    }

    void worker_loop(std::size_t id) {
        std::uint64_t seen_generation = 0;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv_start.wait(lock, [&] { return quit || generation > seen_generation; });
                if (quit) {
                    return;
                }
                seen_generation = generation;
            }

            run_search(id);

            {
                std::lock_guard<std::mutex> lock(mutex);
                active_workers--;
            }
            cv_done.notify_all();
        }
    }

    void run_search(std::size_t id) {
        search_thread_context& ctx = *contexts[id];
        board& chessboard = ctx.pos.chessboard;

        search_data data;
        data.history = ctx.history.get();
        data.main_thread = (id == 0);

        std::string best_move;
        if (chessboard.get_side() == White) {
            data.set_timekeeper(root_info.wtime, root_info.winc, root_info.movestogo, chessboard.move_count(), root_info.max_nodes);
            best_move = iterative_deepening<White>(ctx.pos, data, root_info.max_depth);
        } else {
            data.set_timekeeper(root_info.btime, root_info.binc, root_info.movestogo, chessboard.move_count(), root_info.max_nodes);
            best_move = iterative_deepening<Black>(ctx.pos, data, root_info.max_depth);
        }

        if (id == 0) {
            // main thread is done: stop the helpers, wait for them, then report
            stop_searching();
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv_done.wait(lock, [this] { return active_workers == 1; });
            }
            std::cout << "bestmove " << best_move << std::endl;
        }
    }

    std::vector<std::unique_ptr<native_thread>> threads;
    std::vector<std::unique_ptr<search_thread_context>> contexts;
    time_info root_info;

    std::mutex mutex;
    std::condition_variable cv_start, cv_done;
    std::size_t active_workers = 0;
    std::uint64_t generation = 0;
    bool quit = false;
};

thread_pool search_pool;

#endif //MOTOR_THREAD_POOL_HPP
