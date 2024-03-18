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
#include <atomic>
#include <spdlog/spdlog.h>
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;

const std::string PING_URI_STRING = "/latency.app/1/ping";
const std::string PONG_URI_STRING = "/latency.app/1/32bit";

bool gTerminate = false;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true;
    }
}

class TestLatencyPing : public ::testing::Test, UListener{

    public:

        UStatus onReceive(UMessage &message) const override {
            
            auto payload = message.payload();
    
            const uint64_t *pongTimeMicro = reinterpret_cast<const uint64_t*>(payload.data());
            
                        
            printf("%lu\n", (*pongTimeMicro - pingTimeMicro));

            UStatus status;

            status.set_code(UCode::OK);

            counter.fetch_add(1);
            if (maxSubscribers == counter) {
                cv.notify_one();
            }

            return status;
      
        }

        uint64_t getCurrentTimeMicroseconds() {
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts); // Get current time

            // Convert seconds to microseconds and add nanoseconds converted to microseconds
            return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
        }

         void SetUp() override {
            // Perform initialization before each test case
            ZenohUTransport::instance().registerListener(pongUri, *this);
        }

        void TearDown() override {
            // Perform cleanup after each test case
            ZenohUTransport::instance().unregisterListener(pongUri, *this);
        }

        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }


           // ZenohUTransport::instance().registerListener(rpcUri, TestRPcClient::rpcListener);

        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }

        }
    
        void waitUntilAllCallbacksInvoked() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv.wait(lock, [this](){ return counter >= maxSubscribers; });
        }

        UUri pingUri = LongUriSerializer::deserialize(PING_URI_STRING); //1 pub , many sub 
        UUri pongUri = LongUriSerializer::deserialize(PONG_URI_STRING); //1 sub , many pub

        UUID pingUUid;
        uint64_t pingTimeMicro;
        std::mutex mutex_;
        mutable std::condition_variable cv;
        size_t maxSubscribers = 0;
        mutable std::atomic<size_t> counter = 0;
};

//Send ping to 1 ... n with the ping time + payload
//send pong with with ID + ping time
//map of key= a .. n , value = list of micro seconds results 
TEST_F(TestLatencyPing, LatencyTests1Kb1Sub) {
    
    maxSubscribers = 1;

    auto now = std::chrono::high_resolution_clock::now();
    
    pingUUid = Uuidv8Factory::create();

    UAttributesBuilder builder(pingUUid, UMessageType::UMESSAGE_TYPE_PUBLISH, UPriority::UPRIORITY_CS0);
    UAttributes attributes = builder.build();
        
    for (size_t i = 0 ; i < 1000 ; ++i) {

        
    pingTimeMicro = getCurrentTimeMicroseconds();

    UPayload payload(reinterpret_cast<const uint8_t*>(&pingTimeMicro), sizeof(pingTimeMicro), UPayloadType::VALUE);
    
        std::cout << i << " sending " << pingTimeMicro << std::endl;
        UStatus status = ZenohUTransport::instance().send(pingUri, payload, attributes);
        EXPECT_EQ(status.code(), UCode::OK);

        waitUntilAllCallbacksInvoked();

        counter = 0;
    }
}

TEST_F(TestLatencyPing, LatencyTests1Kb5Sub) {

}

TEST_F(TestLatencyPing, LatencyTests1Kb20Sub) {

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

