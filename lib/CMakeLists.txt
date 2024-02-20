cmake_minimum_required(VERSION 3.20)

project(up-client-zenoh-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_library(
    ${PROJECT_NAME} SHARED
    src/zenohUTransport.cpp
    src/zenohRpcClient.cpp
    src/zenohSessionManager.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE 
                           ${up-cpp_INCLUDE_DIR}
                           include)

target_link_libraries(${PROJECT_NAME} PRIVATE up-cpp::up-cpp ${ZENOH_LIB})
INSTALL(TARGETS ${PROJECT_NAME})
INSTALL(DIRECTORY include DESTINATION .)