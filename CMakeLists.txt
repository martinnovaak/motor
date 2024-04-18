cmake_minimum_required(VERSION 3.28)
project(motor)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS "-march=native")

# Add a custom command to copy nnue.bin to the build directory
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/nnue.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/nnue.bin ${CMAKE_CURRENT_BINARY_DIR}/nnue.bin
        DEPENDS ${CMAKE_SOURCE_DIR}/nnue.bin
        COMMENT "Copying nnue.bin"
)

# Add nnue.bin to the list of source files
set(GENERATED_FILES ${CMAKE_CURRENT_BINARY_DIR}/nnue.bin)

# Add the executable target
add_executable(motor main.cpp ${GENERATED_FILES})
