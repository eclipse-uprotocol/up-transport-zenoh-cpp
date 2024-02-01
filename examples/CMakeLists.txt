project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../uprotocol-cpp/include)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../uprotocol-core-api/)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/../lib)

find_package(spdlog REQUIRED)
find_library(ZENOH_LIB zenohc)
find_package(up-cpp REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

#send-receive server
add_executable(
    demo_server
    send-receive/src/main_server.cpp)

target_link_libraries(
    demo_server
    up-cpp::up-cpp
    ${ZENOH_LIB}
    up-client-zenoh-cpp
    ${PROTOBUF_LIBRARIES}
    fmt
    spdlog::spdlog)

#send-receive client
add_executable(
    demo_client
    send-receive/src/main_client.cpp)

target_link_libraries(
    demo_client
    up-cpp::up-cpp
    ${ZENOH_LIB}
    up-client-zenoh-cpp
    fmt
    spdlog::spdlog)

#rpc server
add_executable(
    demo_rpc_server
    rpc/src/main_rpc_server.cpp)

target_link_libraries(
    demo_rpc_server
    up-cpp::up-cpp
    ${ZENOH_LIB}
    up-client-zenoh-cpp
    ${PROTOBUF_LIBRARIES}
    fmt
    spdlog::spdlog)

#rpc client
add_executable(
    demo_rpc_client
    rpc/src/main_rpc_client.cpp)

target_link_libraries(
    demo_rpc_client
    up-cpp::up-cpp
    ${ZENOH_LIB}
    up-client-zenoh-cpp
    ${PROTOBUF_LIBRARIES}
    fmt
    spdlog::spdlog)
