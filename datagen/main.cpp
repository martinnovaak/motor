#include "datagen.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

std::uint64_t number(const char* value, const char* name) {
    try {
        std::size_t parsed = 0;
        const std::string text(value);
        const auto result = std::stoull(text, &parsed);
        if (parsed != text.size() || result == 0) {
            throw std::invalid_argument("not a positive integer");
        }
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("invalid ") + name + ": " + value);
    }
}

std::size_t position_count(const char* value) {
    if (std::string(value) == "unlimited") {
        return std::numeric_limits<std::size_t>::max();
    }
    const auto result = number(value, "position count");
    if (result > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(std::string("position count is too large: ") + value);
    }
    return static_cast<std::size_t>(result);
}

void usage(const char* executable) {
    std::cerr << "usage: " << executable
              << " <openings.epd|random> <output.bin> [threads] [nodes] [positions]"
                 " [max-plies] [seed]\n"
              << "  positions may be 'unlimited'; stop gracefully with Ctrl-C\n"
              << "  omitted positions processes each EPD opening once\n"
              << "  random generates Viridithas-style random startpos openings and"
                 " requires positions\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 8) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    try {
        if (std::signal(SIGINT, motor::datagen::handle_interrupt) == SIG_ERR) {
            throw std::runtime_error("failed to install Ctrl-C handler");
        }
        motor::datagen::Options options;
        options.random_openings = std::string(argv[1]) == "random";
        if (!options.random_openings) options.openings = argv[1];
        options.output = argv[2];
        if (argc > 3) options.threads = number(argv[3], "thread count");
        if (argc > 4) options.nodes = number(argv[4], "node limit");
        if (argc > 5) options.positions = position_count(argv[5]);
        if (argc > 6) options.max_plies = number(argv[6], "max plies");
        if (argc > 7) options.seed = number(argv[7], "seed");

        motor::datagen::run(options);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "datagen error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
