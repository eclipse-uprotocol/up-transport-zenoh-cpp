cmake_minimum_required(VERSION 3.20)

project(up-client-zenoh-cpp_lib LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Using the "cmake_find_package" and "cmake_paths" generators
if (EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
add_compile_definitions(GLIBCXX_USE_CXX11_ABI=0)

include_directories(include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_library(
    up-client-zenoh-cpp SHARED
    src/zenohUTransport.cpp
    src/zenohRpcClient.cpp
    src/zenohSessionManager.cpp
    src/messageBuilder.cpp
    src/messageParser.cpp)

target_link_libraries(up-client-zenoh-cpp PRIVATE up-cpp::up-cpp ${ZENOH_LIB})
INSTALL(TARGETS up-client-zenoh-cpp)
INSTALL(DIRECTORY include DESTINATION .)