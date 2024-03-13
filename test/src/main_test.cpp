/*
 * Copyright (c) 2024 General Motors GTO LLC
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2024 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csignal>
#include <spdlog/spdlog.h>
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;

class upClientZenohCppTest : public ::testing::Test {

    public:

    public:
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }
        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }
        }
};

/* Deprecate non existing topic */
TEST_F(upClientZenohCppTest, DummyTest) {
   
}

//unsubscribe from non existane topic 
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}