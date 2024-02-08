project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(pubsub)
add_subdirectory(rpc)
add_subdirectory(subscription)