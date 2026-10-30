#include "datagen.hpp"

#include <cstdlib>
#include <iostream>
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

void usage(const char* executable) {
    std::cerr << "usage: " << executable
              << " <openings.epd> <output.bin> [threads] [nodes] [games] [max-plies]\n"
              << "  games defaults to the number of EPD positions\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 7) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    try {
        motor::datagen::Options options;
        options.openings = argv[1];
        options.output = argv[2];
        if (argc > 3) options.threads = number(argv[3], "thread count");
        if (argc > 4) options.nodes = number(argv[4], "node limit");
        if (argc > 5) options.games = number(argv[5], "game count");
        if (argc > 6) options.max_plies = number(argv[6], "max plies");

        motor::datagen::run(options);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "datagen error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
