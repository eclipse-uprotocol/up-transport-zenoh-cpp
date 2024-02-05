cmake_minimum_required(VERSION 3.20)

project(up-client-zenoh-cpp_lib LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Using the "cmake_find_package" and "cmake_paths" generators
if (EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)

include_directories(include)
include_directories(${up-cpp_INCLUDE_DIRS})
include_directories(${up-cpp_INCLUDE_DIRS}/up-core-api)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_library(
    up-client-zenoh-cpp SHARED
    src/uSubscriptionClient.cpp
    src/zenohUTransport.cpp
    src/zenohRpcClient.cpp
    src/zenohSessionManager.cpp
    src/messageBuilder.cpp
    src/messageParser.cpp)

target_link_libraries(up-client-zenoh-cpp PRIVATE up-cpp::up-cpp ${ZENOH_LIB})
INSTALL(TARGETS up-client-zenoh-cpp)
INSTALL(DIRECTORY include DESTINATION .)