#include "datagen.hpp"
#include <iostream>
#include <signal.h>

using namespace motor::datagen;

// Global pointer for signal handling
DataGenerator* g_generator = nullptr;

void signalHandler(int signal) {
    if (g_generator && (signal == SIGINT || signal == SIGTERM)) {
        std::cout << "\nReceived interrupt signal. Stopping data generation gracefully..." << std::endl;
        g_generator->stopGeneration();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Motor Chess Engine - Data Generation\n"
              << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -g, --games <num>       Number of games to play (default: 1000)\n"
              << "  -t, --threads <num>     Number of threads (default: 4)\n" 
              << "  -d, --depth <num>       Search depth (default: 8)\n"
              << "  -o, --output <file>     Output file (default: training_data.txt)\n"
              << "  -r, --random <min-max>  Random opening moves (default: 8-9)\n"
              << "  -v, --verbose           Verbose output\n"
              << "  -h, --help             Show this help\n\n"
              << "Examples:\n"
              << "  " << program_name << " -g 5000 -t 8 -d 6\n"
              << "  " << program_name << " --games 10000 --output data.txt --verbose\n"
              << std::endl;
}

DataGenConfig parseArgs(int argc, char* argv[]) {
    DataGenConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if ((arg == "-g" || arg == "--games") && i + 1 < argc) {
            config.num_games = std::stoi(argv[++i]);
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            config.num_threads = std::stoi(argv[++i]);
        } else if ((arg == "-d" || arg == "--depth") && i + 1 < argc) {
            config.search_depth = std::stoi(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if ((arg == "-r" || arg == "--random") && i + 1 < argc) {
            std::string range = argv[++i];
            auto dash = range.find('-');
            if (dash != std::string::npos) {
                config.min_random_moves = std::stoi(range.substr(0, dash));
                config.max_random_moves = std::stoi(range.substr(dash + 1));
            }
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    try {
        DataGenConfig config = parseArgs(argc, argv);
        
        // Set up signal handlers for graceful shutdown
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        std::cout << "Motor Chess Engine - Data Generation\n" 
                  << "=====================================" << std::endl;
        
        DataGenerator generator(config);
        g_generator = &generator;
        
        auto start_time = std::chrono::steady_clock::now();
        generator.generateData();
        auto end_time = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\nData generation completed!" << std::endl;
        printDataGenStats(generator);
        std::cout << "Total time: " << formatDuration(duration) << std::endl;
        std::cout << "Output file: " << config.output_file << std::endl;
        
        g_generator = nullptr;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
