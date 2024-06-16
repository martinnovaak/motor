#include "cli/uci.hpp"

int main (int argv, char* argc[]) {
    if (argv > 1 && std::string{ argc[1] } == "bench") {
        bench(12);
        return 0;
    }

    uci_mainloop();

    return 0;
}