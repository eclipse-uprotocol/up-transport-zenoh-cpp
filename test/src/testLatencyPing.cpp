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
#include <up-client-zenoh-cpp/client/upZenohClient.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>
#include <iostream>
#include <filesystem>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::client;

const std::string PING_URI_STRING = "/latency.app/1/ping";
const std::string PONG_URI_STRING = "/latency.app/1/pong";

bool gTerminate = false;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true;
    }
}

struct LatencyPerPID {
    uint64_t latencyTotal;
    uint64_t peakLatency;
    uint64_t minLatency;
    size_t samplesCount;
};

class TestLatencyPing : public ::testing::Test, UListener{

    public:

        UStatus onReceive(UMessage &message) const override {
            
            UStatus status;

            status.set_code(UCode::INTERNAL);

            auto payload = message.payload();

            if ((sizeof(uint64_t) + sizeof(pid_t)) != payload.size()){
                spdlog::error("wrong payload size received {} expected {}" , payload.size(), (sizeof(uint64_t) + sizeof(pid_t)));
                return status;
            }

            responseCounterTotal_.fetch_add(1);
            responseCounter_.fetch_add(1);

            /* start the measurments after all "warmup" messages recevied */
            if (responseCounterTotal_.load() >= (numOfWarmupMessages_ * maxSubscribers_)) {
               
                uint64_t pongTimeMicro;
                pid_t pid;

                memcpy(&pongTimeMicro, payload.data(), sizeof(pongTimeMicro));
                memcpy(&pid, payload.data() + sizeof(pingTimeMicro), sizeof(pid));
                
                latencyTotal_.fetch_add(pongTimeMicro - pingTimeMicro);

                LatencyPerPID entry = processTable[pid];
                uint64_t latency = pongTimeMicro - pingTimeMicro;

                /* store the peak latency */
                if (entry.peakLatency < latency){
                    entry.peakLatency = latency;
                }

                /* store the min latency */
                if ((entry.minLatency > latency) || (entry.minLatency == 0 )) {
                    entry.minLatency = latency;
                }

                entry.latencyTotal += (pongTimeMicro - pingTimeMicro);
                entry.samplesCount += 1;

                processTable[pid] = entry;
            }

            /* notfiy the main thread when all messages received*/
            if (maxSubscribers_ == responseCounter_) {
                cv.notify_one();
            }

            status.set_code(UCode::OK);         

            return status;
        }

        uint64_t getCurrentTimeMicroseconds() {
          
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto duration = currentTime.time_since_epoch();
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

            return microseconds;
        }

        std::vector<int> getPids() {

            std::vector<int> pidList;
            char buffer[128];
            std::shared_ptr<FILE> pipe(popen("ps -ef | grep testLatencyPong | grep -v grep | awk '{print $2}'", "r"), pclose);
            if (!pipe) throw std::runtime_error("popen() failed!");

            while (!feof(pipe.get())) {
                if (fgets(buffer, 128, pipe.get()) != nullptr) {
                    /* Split the output into tokens and store the PID */
                    std::istringstream iss(buffer);
                    int pid;
                    if (iss >> pid) {
                        pidList.push_back(pid);
                    }
                }
            }

            return pidList;
        }

        void runTest(size_t bufferSize, 
                     size_t numSub) {

            maxSubscribers_ = numSub;
            responseCounter_ = 0;
            latencyTotal_ = 0;
            responseCounterTotal_ = 0;
            
            processTable.clear();

            uint8_t payloadBuffer[bufferSize];

            /* the testLatencyPong exec should be in the same folder as the test latency ping */
            std::string currentPath = std::filesystem::current_path().string();

            currentPath.append("/testLatencyPong > /dev/null &");
           
            for (size_t i = 0 ; i < maxSubscribers_ ; ++i) {
                std::cout << currentPath << std::endl;
                std::system(currentPath.c_str());
            }
            
            std::vector<int> pidList = getPids();

            EXPECT_EQ(pidList.size(), maxSubscribers_);

            for (int pid : pidList) {
                LatencyPerPID entry;

                entry.latencyTotal = 0;
                entry.samplesCount = 0;
                entry.peakLatency = 0;
                entry.minLatency = 0;
                processTable[pid] = entry;
            }

            spdlog::info("Sleeping for 5 seconds to give time for processes to initialize");
            sleep(5);
            
            auto instance = UpZenohClient::instance();

            EXPECT_NE(instance , nullptr);

            auto status = instance->registerListener(pongUri, *this);    

            EXPECT_EQ(status.code(), UCode::OK);

            for (size_t i = 0 ; i < numOfmessages_ ; ++i) {

                auto builder = UAttributesBuilder::publish(pingUri, UPriority::UPRIORITY_CS0);
                
                UAttributes attributes = builder.build();

                pingTimeMicro = getCurrentTimeMicroseconds();

                UPayload payload(payloadBuffer, bufferSize, UPayloadType::REFERENCE);          
   
                UMessage message(payload, attributes);

                UStatus status = instance->send(message);
                
                EXPECT_EQ(status.code(), UCode::OK);

                waitUntilAllCallbacksInvoked();

                responseCounter_ = 0;
            }

            spdlog::info("Sleeping for 5 seconds to finish get all of the responses");
            sleep (5);
            spdlog::info("*** message size , total samples received , total latency , average latency  ***" );
            spdlog::info("***  {} , \t\t{} , \t\t\t{} , \t{} ***" , 
                bufferSize, responseCounterTotal_.load(), latencyTotal_.load() , (latencyTotal_.load() / (responseCounterTotal_.load()/2))); 

            spdlog::info("*** PID , \t\tsamples count , total latency , average latency , peak latency , minimum latency  ***");
            for (int pid : pidList) {

                LatencyPerPID entry = processTable[pid];
                spdlog::info("*** {} , \t\t{} , \t\t\t{} , \t{} , \t\t{} , \t\t{} ***", 
                    pid, entry.samplesCount, entry.latencyTotal, entry.latencyTotal / entry.samplesCount, entry.peakLatency, entry.minLatency);
            }

            for (int pid : pidList) {
                int result = kill(pid, SIGINT);
                if (result != 0) {
                    spdlog::error("Failed to send SIGINT signal to process with PID {}", pid);
                }
            }

            spdlog::info("Sleeping for 5 seconds to all processes to terminate");
            sleep(5);

            /* if any of the processes did not gratefuly terminated , kill -9 it */
            pidList = getPids();

            if (0 != pidList.size()) {
                for (int pid : pidList) {
                   kill(pid, SIGKILL);
                }
                spdlog::info("Sleeping for 5 seconds to all processes to terminate");
                sleep(5);
            }

            pidList = getPids();

            EXPECT_EQ(pidList.size(), 0);

            status = instance->unregisterListener(pongUri, *this);

            EXPECT_EQ(status.code(), UCode::OK);
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
        mutable std::atomic<uint64_t> latencyTotal_ = 0;
        mutable std::atomic<size_t> responseCounterTotal_ = 0;
        mutable std::unordered_map<pid_t, LatencyPerPID> processTable;
        static constexpr auto numOfmessages_ = size_t(2000);
        static constexpr auto numOfWarmupMessages_ = size_t(numOfmessages_ / 2);
};

TEST_F(TestLatencyPing, LatencyTests1Kb1Sub) {
    runTest(1024, 1);
}

TEST_F(TestLatencyPing, LatencyTests1Kb5Sub) {
    runTest(1024, 5);
}

TEST_F(TestLatencyPing, LatencyTests1Kb20Sub) {
    runTest(1024, 20);
}

TEST_F(TestLatencyPing, LatencyTests10Kb1Sub) {
    runTest(1024 * 10 , 1);
}

TEST_F(TestLatencyPing, LatencyTests10Kb5Sub) {
    runTest(1024 * 10 , 5);
}

TEST_F(TestLatencyPing, LatencyTests10Kb20Sub) {
    runTest(1024 * 10, 20);
}

TEST_F(TestLatencyPing, LatencyTests100Kb1Sub) {
    runTest(1024 * 100, 1);
}

TEST_F(TestLatencyPing, LatencyTests100Kb5Sub) {
    runTest(1024 * 100, 5);
}

TEST_F(TestLatencyPing, LatencyTests100Kb20Sub) {
    runTest(1024 * 100, 20);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

