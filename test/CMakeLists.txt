# Copyright (c) 2024 General Motors GTO LLC
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
#
# SPDX-FileType: SOURCE
# SPDX-FileCopyrightText: 2024 General Motors GTO LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(test VERSION 0.1.0 LANGUAGES CXX)

if(BUILD_UNBUNDLED)
    find_package(zenohc REQUIRED)
    set(ZENOH_LIBRARY zenohc::lib)
else()
    if(CROSS_COMPILE)
        find_package(zenohc REQUIRED)
	set(ZENOH_LIBRARY zenohc::lib)
    else()
        find_library(ZENOH_LIB zenohc)
	set(ZENOH_LIBRARY ${ZENOH_LIB})
    endif()
endif()

find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONNENTS interprocess)

add_executable(testRpcClient
	src/testRpcClient.cpp)

 target_include_directories(testRpcClient
	PUBLIC
		$ENV{CMAKE_ZENOH_INCLUDE_PATH}
		${Boost_INCLUDE_DIR}
)
  
target_link_libraries(testRpcClient 
	PUBLIC
		up-client-zenoh-cpp::up-client-zenoh-cpp
	    spdlog::spdlog
	PRIVATE
		GTest::gtest_main
		GTest::gmock    
		pthread
  		${ZENOH_LIBRARY}
)

add_test("t-18-testRpcClient" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testRpcClient)

add_executable(testZenohUri
	src/testZenohUri.cpp)
target_link_libraries(testZenohUri 
	PUBLIC
		up-client-zenoh-cpp::up-client-zenoh-cpp
		spdlog::spdlog
	PRIVATE
		GTest::gtest_main
		GTest::gmock
)

add_test("t-18-testZenohUri" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testZenohUri)

add_executable(testLatencyPing
	src/testLatencyPing.cpp)

 target_include_directories(testLatencyPing
	PUBLIC
		$ENV{CMAKE_ZENOH_INCLUDE_PATH})
  
target_link_libraries(testLatencyPing 
	PUBLIC
		up-client-zenoh-cpp::up-client-zenoh-cpp
        spdlog::spdlog
	PRIVATE
		GTest::gtest_main
		GTest::gmock    
		pthread
  		${ZENOH_LIBRARY}
		${Boost_LIBRARIES}
)

add_test("t-18-testLatencyPing" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testLatencyPing)

add_executable(testLatencyPong
	src/testLatencyPong.cpp)

 target_include_directories(testLatencyPong
	PUBLIC
		$ENV{CMAKE_ZENOH_INCLUDE_PATH})
  
target_link_libraries(testLatencyPong 
	PUBLIC
		up-client-zenoh-cpp::up-client-zenoh-cpp
        spdlog::spdlog
	PRIVATE
		pthread
  		${ZENOH_LIBRARY}
)

add_test("t-18-testPubSub" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testPubSub)

add_executable(testPubSub
	src/testPubSub.cpp)

 target_include_directories(testPubSub
	PUBLIC
		$ENV{CMAKE_ZENOH_INCLUDE_PATH}
		${Boost_INCLUDE_DIR}
)
  
target_link_libraries(testPubSub 
	PUBLIC
		up-client-zenoh-cpp::up-client-zenoh-cpp
        spdlog::spdlog
	PRIVATE
		GTest::gtest_main
		GTest::gmock
		pthread
  		${ZENOH_LIBRARY}
)

