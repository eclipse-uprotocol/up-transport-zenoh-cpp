# Copyright (c) 2023 General Motors GTO LLC
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
cmake_minimum_required(VERSION 3.20)
project(pubsub VERSION 0.1.0 LANGUAGES CXX)


if(BUILD_UNBUNDLED)
    find_package(up-cpp REQUIRED)
    find_package(zenohc REQUIRED)
    find_package(spdlog REQUIRED)
else()
    find_library(ZENOH_LIB zenohc)
    find_package(up-cpp REQUIRED)
    find_package(up-client-zenoh-cpp REQUIRED)
    find_package(fmt REQUIRED)
    find_package(spdlog REQUIRED)
    find_package(Protobuf REQUIRED)
endif()

add_definitions(-DSPDLOG_FMT_EXTERNAL)

add_executable(sub src/main_sub.cpp)

if(BUILD_UNBUNDLED)
    target_link_libraries(sub
        PRIVATE
            up-cpp::up-cpp
            up-client-zenoh-cpp::up-client-zenoh-cpp
            up-client-zenoh-cpp::up-core-api-protos
            zenohc::lib)
else()
    target_link_libraries(
        sub
        up-client-zenoh-cpp::up-client-zenoh-cpp
        up-cpp::up-cpp
        fmt::fmt
        spdlog::spdlog
        ${ZENOH_LIB})

    target_include_directories(sub 
        PRIVATE 
            ${up-client-zenoh-cpp_INCLUDE_DIR}
            ${spdlog_INCLUDE_DIR})
endif()



add_executable(pub src/main_pub.cpp)

if(BUILD_UNBUNDLED)
    target_link_libraries(pub
        PRIVATE
            up-cpp::up-cpp
            up-client-zenoh-cpp::up-client-zenoh-cpp
            up-client-zenoh-cpp::up-core-api-protos
            zenohc::lib)

else()
    target_link_libraries(pub
        up-client-zenoh-cpp::up-client-zenoh-cpp
        up-cpp::up-cpp
        fmt::fmt
        spdlog::spdlog
        ${ZENOH_LIB})

    target_include_directories(pub 
        PRIVATE 
            ${up-client-zenoh-cpp_INCLUDE_DIR}
            ${spdlog_INCLUDE_DIR})
endif()
