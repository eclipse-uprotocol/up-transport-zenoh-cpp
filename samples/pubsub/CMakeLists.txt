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
    sub
    src/main_sub.cpp)

target_link_libraries(
    sub
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    fmt::fmt
    spdlog::spdlog
    ${ZENOH_LIB})

message("MICHAEL ${up-client-zenoh-cpp_INCLUDE_DIR}")

target_include_directories(sub PRIVATE 
                           ${up-client-zenoh-cpp_INCLUDE_DIR}
                           ${spdlog_INCLUDE_DIR})

add_executable(
    pub
    src/main_pub.cpp)
  
target_link_libraries(
    pub
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    fmt::fmt
    spdlog::spdlog
    ${ZENOH_LIB})

target_include_directories(pub PRIVATE 
                           ${up-client-zenoh-cpp_INCLUDE_DIR}
                           ${spdlog_INCLUDE_DIR})