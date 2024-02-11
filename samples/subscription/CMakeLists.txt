cmake_minimum_required(VERSION 3.20)
project(pubsub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)
find_package(up-client-zenoh-cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Protobuf REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_executable(
    sub_client
    src/main_sub_client.cpp)

target_link_libraries(
    sub_client
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    fmt::fmt
    spdlog::spdlog
    ${ZENOH_LIB})

target_include_directories(sub_client PRIVATE 
                           ${up-cpp_INCLUDE_DIRS}/up-core-api
                           ${up-client-zenoh-cpp_INCLUDE_DIR}
                           ${spdlog_INCLUDE_DIR})
