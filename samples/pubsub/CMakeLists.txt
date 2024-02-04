cmake_minimum_required(VERSION 3.20)
project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
add_compile_definitions(GLIBCXX_USE_CXX11_ABI=0)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)
find_package(up-client-zenoh-cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_executable(
    sub
    src/main_sub.cpp)

target_link_libraries(
    sub
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    ${ZENOH_LIB}
    fmt
    spdlog::spdlog)

#rpc client
add_executable(
    pub
    src/main_pub.cpp)

target_link_libraries(
    pub
    up-client-zenoh-cpp::up-client-zenoh-cpp
    up-cpp::up-cpp
    ${ZENOH_LIB}
    fmt
    spdlog::spdlog)
