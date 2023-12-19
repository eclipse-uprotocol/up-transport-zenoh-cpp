project(demo LANGUAGES CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../uprotocol-cpp/include)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../uprotocol-core-api/)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/../lib)

find_package(spdlog REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

#send-receive server
add_executable(
    demo_server 
    send-receive/src/main_server.cpp)

target_link_libraries(
    demo_server 
    uprotocol-cpp-ulink-zenoh 
    uprotocol-cpp
    /usr/local/lib64/libzenohc.so
    fmt
    spdlog::spdlog)

#send-receive client
add_executable(
    demo_client 
    send-receive/src/main_client.cpp)

target_link_libraries(
    demo_client 
    uprotocol-cpp-ulink-zenoh 
    uprotocol-cpp
    /usr/local/lib64/libzenohc.so
    fmt
    spdlog::spdlog)

#rpc server
add_executable(
    demo_rpc_server 
    rpc/src/main_rpc_server.cpp)

target_link_libraries(
    demo_rpc_server 
    uprotocol-cpp-ulink-zenoh 
    uprotocol-cpp
    /usr/local/lib64/libzenohc.so
    fmt
    spdlog::spdlog)

#rpc client
add_executable(
    demo_rpc_client 
    rpc/src/main_rpc_client.cpp)

target_link_libraries(
    demo_rpc_client
    uprotocol-cpp-ulink-zenoh 
    uprotocol-cpp
    /usr/local/lib64/libzenohc.so
    fmt
    spdlog::spdlog)