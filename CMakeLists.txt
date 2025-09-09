cmake_minimum_required(VERSION 3.14)
project(Motor CXX)

set(CMAKE_CXX_STANDARD 20)

# Core library
add_library(motor_core STATIC
        src/core/attacks.cpp
        src/core/board.cpp
)
target_include_directories(motor_core PUBLIC src/core)

# Main executable
add_executable(motor_app src/main.cpp)
target_link_libraries(motor_app motor_core)

# Tests
include(FetchContent)
FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.9.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(motor_tests
        tests/attacks_tests.cpp
        tests/rays_tests.cpp
        tests/zobrist_tests.cpp
        tests/chess_move_tests.cpp
        tests/board_tests.cpp
)
target_link_libraries(motor_tests motor_core Catch2::Catch2WithMain)

enable_testing()
add_test(NAME MotorTests COMMAND motor_tests)
