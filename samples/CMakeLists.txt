cmake_minimum_required(VERSION 3.20)
project(samples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(pubsub)
add_subdirectory(rpc)