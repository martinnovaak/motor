cmake_minimum_required(VERSION 3.28)
project(motor)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS "-march=native")

set(EVALFILE "" CACHE FILEPATH "NNUE network to embed")
if(NOT EVALFILE)
        set_property(
                DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${CMAKE_SOURCE_DIR}/net.txt"
        )
        file(READ "${CMAKE_SOURCE_DIR}/net.txt" MOTOR_NETWORK_URL)
        string(STRIP "${MOTOR_NETWORK_URL}" MOTOR_NETWORK_URL)
        if(NOT MOTOR_NETWORK_URL MATCHES "^https://")
                message(FATAL_ERROR "net.txt must contain an HTTPS URL")
        endif()

        string(SHA256 MOTOR_NETWORK_ID "${MOTOR_NETWORK_URL}")
        set(EVALFILE "${CMAKE_BINARY_DIR}/nnue-${MOTOR_NETWORK_ID}.bin")
        if(NOT EXISTS "${EVALFILE}")
                file(
                        DOWNLOAD "${MOTOR_NETWORK_URL}" "${EVALFILE}"
                        TLS_VERIFY ON
                        STATUS MOTOR_NETWORK_STATUS
                        SHOW_PROGRESS
                        INACTIVITY_TIMEOUT 30
                        TIMEOUT 300
                )
                list(GET MOTOR_NETWORK_STATUS 0 MOTOR_NETWORK_STATUS_CODE)
                if(NOT MOTOR_NETWORK_STATUS_CODE EQUAL 0)
                        list(GET MOTOR_NETWORK_STATUS 1 MOTOR_NETWORK_STATUS_MESSAGE)
                        file(REMOVE "${EVALFILE}")
                        message(FATAL_ERROR
                                "Failed to download NNUE: ${MOTOR_NETWORK_STATUS_MESSAGE}")
                endif()
        endif()
endif()

get_filename_component(EVALFILE "${EVALFILE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

add_executable(motor main.cpp)
target_compile_definitions(motor PRIVATE NNUE_FILE="${EVALFILE}")

# Rebuild when the embedded network changes
set_property(SOURCE main.cpp APPEND PROPERTY OBJECT_DEPENDS "${EVALFILE}")
