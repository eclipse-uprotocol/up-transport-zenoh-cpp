project(uprotocol-cpp-ulink-zenoh LANGUAGES CXX)

include_directories(include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../uprotocol-cpp/include)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../uprotocol-core-api/)

find_library(ZENOH_LIB zenohc)

find_package(spdlog REQUIRED)

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_subdirectory(examples)

add_library(
    ${PROJECT_NAME} SHARED 
    src/zenohUTransport.cpp
    src/zenohRpcClient.cpp
    src/zenohSessionManager.cpp
    src/messageBuilder.cpp
    src/messageParser.cpp
    /usr/local/lib64/libzenohc.so)

