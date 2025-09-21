#include "benchmarks/perft.hpp"
#include "board.hpp"
#include "cli/uci.hpp"

int main() {
    using namespace motor;
    using namespace motor::benchmarks;

    // The engine can now play chess!
    UCI uci;
    uci.loop();  // Starts UCI protocol handling

    return 0;
}
