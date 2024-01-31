cmake_minimum_required(VERSION 3.26)
project(up-client-zenoh-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
# Using the "cmake_find_package" and "cmake_paths" generators
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
 
find_package(up-cpp REQUIRED)

include_directories(include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

find_library(ZENOH_LIB zenohc)
#find_library(UPCPP_LIB up-cpp)
find_package(up-cpp REQUIRED)

find_package(spdlog REQUIRED)
find_package(Protobuf REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_subdirectory(examples)

add_library(
    ${PROJECT_NAME} SHARED 
    src/zenohUTransport.cpp
    src/zenohRpcClient.cpp
    src/zenohSessionManager.cpp
    src/messageBuilder.cpp
    src/messageParser.cpp
    ${ZENOH_LIB})

target_link_libraries(${PROJECT_NAME} PRIVATE  up-cpp::up-cpp)

