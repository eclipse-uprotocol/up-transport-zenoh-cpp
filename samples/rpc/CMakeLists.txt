cmake_minimum_required(VERSION 3.20)
project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)
find_package(up-client-zenoh-cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_executable(
    rpc_server
    src/main_rpc_server.cpp)

target_link_libraries(
    rpc_server
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    ${ZENOH_LIB}
    fmt
    spdlog::spdlog)

#rpc client
add_executable(
    rpc_client
    src/main_rpc_client.cpp)

target_link_libraries(
    rpc_client
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    ${ZENOH_LIB}
    fmt
    spdlog::spdlog)
