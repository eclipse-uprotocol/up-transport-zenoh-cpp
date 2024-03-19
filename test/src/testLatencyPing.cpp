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
            
            const uint64_t *pongTimeMicro = reinterpret_cast<const uint64_t*>(message.payload().data());
            
            latencyTotal_ += *pongTimeMicro - pingTimeMicro;

            responseCounter_.fetch_add(1);
            ++responseCounterTotal_;

            if (maxSubscribers_ == responseCounter_) {
                cv.notify_one();
            }

            UStatus status;

            status.set_code(UCode::OK);         

            return status;
        }

        uint64_t getCurrentTimeMicroseconds() {
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts); 

            return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
        }

        std::vector<int> getPids() {
            std::vector<int> pidList;
            char buffer[128];
            std::shared_ptr<FILE> pipe(popen("ps -ef | grep testLatencyPong | grep -v grep | awk '{print $2}'", "r"), pclose);
            if (!pipe) throw std::runtime_error("popen() failed!");

            while (!feof(pipe.get())) {
                if (fgets(buffer, 128, pipe.get()) != nullptr) {
                    // Split the output into tokens and store the PID
                    std::istringstream iss(buffer);
                    int pid;
                    if (iss >> pid) {
                        pidList.push_back(pid);
                    }
                }
            }
            return pidList;
        }

        void SetUp() override {
            ZenohUTransport::instance().registerListener(pongUri, *this);
        }

        void TearDown() override {
            ZenohUTransport::instance().unregisterListener(pongUri, *this);
        }

        static void SetUpTestSuite() {
            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }
        }

        static void TearDownTestSuite() {
            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }
        }
    
        void waitUntilAllCallbacksInvoked() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv.wait(lock, [this](){ return responseCounter_ >= maxSubscribers_; });
        }

        UUri pingUri = LongUriSerializer::deserialize(PING_URI_STRING); //1 pub , many sub 
        UUri pongUri = LongUriSerializer::deserialize(PONG_URI_STRING); //1 sub , many pub

        uint64_t pingTimeMicro;
        std::mutex mutex_;
        mutable std::condition_variable cv;
        size_t maxSubscribers_ = 0;
        mutable std::atomic<size_t> responseCounter_ = 0;
        mutable uint64_t latencyTotal_ = 0;
        mutable size_t responseCounterTotal_ = 0;
};

TEST_F(TestLatencyPing, LatencyTests1Kb1Sub) {
        
    maxSubscribers_ = 1;
    responseCounter_ = 0;
    latencyTotal_ = 0;
    responseCounterTotal_ = 0;

    for (size_t i = 0 ; i < maxSubscribers_ ; ++i) {
        std::system(LATENCY_PONG_BIN_PATH );
    }
    
    std::vector<int> pidList = getPids();

    EXPECT_EQ(pidList.size(), maxSubscribers_);

    spdlog::info("Sleeping for 10 seconds to give time for processes to initialize");
    sleep(10);
    
    for (size_t i = 0 ; i < 1000 ; ++i) {

        auto pingUUid = Uuidv8Factory::create();

        UAttributesBuilder builder(pingUUid, UMessageType::UMESSAGE_TYPE_PUBLISH, UPriority::UPRIORITY_CS0);
        UAttributes attributes = builder.build();     
        
        pingTimeMicro = getCurrentTimeMicroseconds();

        UPayload payload(reinterpret_cast<const uint8_t*>(&pingTimeMicro), sizeof(pingTimeMicro), UPayloadType::VALUE);
    
        UStatus status = ZenohUTransport::instance().send(pingUri, payload, attributes);
        
        EXPECT_EQ(status.code(), UCode::OK);

        waitUntilAllCallbacksInvoked();

        responseCounter_ = 0;
    }

    spdlog::info("Sleeping for 10 seconds to finish get all of the responses");
    sleep (10);
    spdlog::info("total samples received {}  total latency {} average latency {}" , responseCounterTotal_, latencyTotal_ , (latencyTotal_ / responseCounterTotal_)); 

    for (int pid : pidList) {
        int result = kill(pid, SIGINT);
        if (result == 0) {
            spdlog::info("Sent SIGINT signal to process with PID {}", pid);
        } else {
            spdlog::error("Failed to send SIGINT signal to process with PID {}", pid);
        }
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

