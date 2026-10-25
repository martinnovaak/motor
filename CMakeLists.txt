cmake_minimum_required(VERSION 3.28)
project(motor)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS "-march=native")

add_executable(motor main.cpp)

# Rebuild when the embedded network changes (#embed in evaluation/nnue.hpp)
set_property(SOURCE main.cpp APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/nnue.bin)
