cmake_minimum_required(VERSION 3.20)
project(rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)
find_package(up-client-zenoh-cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Protobuf REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_executable(
    rpc_server
    src/main_rpc_server.cpp)

target_link_libraries(
    rpc_server
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    fmt::fmt
    spdlog::spdlog
    ${ZENOH_LIB})

target_include_directories(rpc_server PRIVATE 
                           ${up-client-zenoh-cpp_INCLUDE_DIR}
                           ${spdlog_INCLUDE_DIR})

#rpc client
add_executable(
    rpc_client
    src/main_rpc_client.cpp)

target_link_libraries(
    rpc_client
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    fmt::fmt
    spdlog::spdlog
    ${ZENOH_LIB})

target_include_directories(rpc_client PRIVATE 
                           ${up-client-zenoh-cpp_INCLUDE_DIR}
                           ${spdlog_INCLUDE_DIR})