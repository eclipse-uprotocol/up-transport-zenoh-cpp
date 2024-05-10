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

cmake_minimum_required(VERSION 3.16)
project(test VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_library( DL_LIBRARY libdl.so  )
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# if(BUILD_UNBUNDLED)
#     find_package(zenohc REQUIRED)
#     set(ZENOH_LIBRARY zenohc::lib)
# else()
#     if(CROSS_COMPILE)
#         find_package(zenohc REQUIRED)
# 	set(ZENOH_LIBRARY zenohc::lib)
#     else()
#         find_library(ZENOH_LIB zenohc)
# 	set(ZENOH_LIBRARY ${ZENOH_LIB})
#     endif()
# endif()

# find_package(GTest REQUIRED)

add_executable(api_test
	src/api_test.cpp)
target_link_libraries(api_test 
	PUBLIC
		up-client-zenoh-cpp::api_implementation
		${DL_LIBRARY}
)

