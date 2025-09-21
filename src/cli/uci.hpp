#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include "coordinator.hpp"
#include "search.hpp"

namespace motor {
    class Coordinator;
    class Search;

    class UCI {
    private:
        std::unique_ptr<Coordinator> coordinator;
        std::unique_ptr<Search> searcher;

    public:
        UCI();
        
        // Main UCI loop
        void loop();
        
        // UCI command handlers
        void handleUCI();
        void handleIsReady();
        void handlePosition(const std::string& command);
        void handleGo(const std::string& command);
        void handleStop();
        void handleQuit();
        void handleUCINewGame();
        void handleSetOption(const std::string& command);
        void handleEval();
        void handleD(); // Display board
        
        // Utility methods
        void sendResponse(const std::string& response);
        std::vector<std::string> splitCommand(const std::string& command);
        
        // Engine info
        static constexpr const char* ENGINE_NAME = "Motor";
        static constexpr const char* ENGINE_VERSION = "v1";

    private:
        void setupPosition(const std::string& fen, const std::vector<std::string>& moves);
        ChessMove parseMove(const std::string& moveStr);
        void printBoard();
    };

} // namespace motor
