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
#include <filesystem>
#include <boost/process.hpp>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::client;
using namespace boost::process;

const std::string PING_URI_STRING = "/latency.app/1/ping";
const std::string PONG_URI_STRING = "/latency.app/1/pong";

struct LatencyPerPID {
    uint64_t latencyTotal;
    uint64_t peakLatency;
    uint64_t minLatency;
    size_t samplesCount;
};

class TestLatencyPing : public ::testing::Test, UListener {

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

                LatencyPerPID entry = processTable_[pid];
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

                processTable_[pid] = entry;
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

        void runGTest() {

            uint8_t payloadBuffer[payloadSize_];

            std::vector<child> childProcVector;

            responseCounter_ = 0;
            latencyTotal_ = 0;
            responseCounterTotal_ = 0;
            
            processTable_.clear();
            /* the testLatencyPong exec should be in the same folder as the test latency ping */
            std::string currentPath = std::filesystem::current_path().string();

            currentPath.append("/testLatencyPong > /dev/null &");
           
            for (size_t i = 0 ; i < maxSubscribers_ ; ++i) {

                child childProc(currentPath);

                childProcVector.push_back(std::move(childProc));
            }
            
            for (const auto&  childProc: childProcVector) {              
                LatencyPerPID entry = LatencyPerPID{};;
                processTable_[childProc.id()] = entry;
            }
          
            auto instance = UpZenohClient::instance();
            spdlog::info("sleeping for 5 seconds to allow session creation");
            sleep(5);
            spdlog::info("Starting test");
           
            EXPECT_EQ(childProcVector.size(), maxSubscribers_);
            EXPECT_NE(instance, nullptr);
            EXPECT_EQ(instance->registerListener(pongUri, *this).code(), UCode::OK);

            for (size_t i = 0 ; i < numOfmessages_ ; ++i) {

                auto builder = UAttributesBuilder::publish(pingUri, UPriority::UPRIORITY_CS0);
                
                UAttributes attributes = builder.build();

                pingTimeMicro = getCurrentTimeMicroseconds();

                UPayload payload(payloadBuffer, payloadSize_, UPayloadType::REFERENCE);          
   
                UMessage message(payload, attributes);
              
                EXPECT_EQ(instance->send(message).code(), UCode::OK);

                waitForResponse();

                responseCounter_ = 0;
            }

            spdlog::info("*** message size , total samples received , total latency , average latency  ***" );
            spdlog::info("***  {} , \t\t{} , \t\t\t{} , \t{} ***" , 
                payloadSize_, responseCounterTotal_.load(), latencyTotal_.load() , (latencyTotal_.load() / (responseCounterTotal_.load()/2))); 

            spdlog::info("*** PID , \t\tsamples count , total latency , average latency , peak latency , minimum latency  ***");
            for (const auto& childProc: childProcVector) {    

                LatencyPerPID entry = processTable_[childProc.id()];
                spdlog::info("*** {} , \t\t{} , \t\t\t{} , \t{} , \t\t{} , \t\t{} ***", 
                    childProc.id(), entry.samplesCount, entry.latencyTotal, entry.latencyTotal / entry.samplesCount, entry.peakLatency, entry.minLatency);
            }

            for (auto& childProc: childProcVector) {          
                childProc.terminate();
                childProc.wait();
            }

            EXPECT_EQ(instance->unregisterListener(pongUri, *this).code(), UCode::OK);
        }
    
        void waitForResponse() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv.wait(lock, [this](){ return responseCounter_ >= maxSubscribers_; });
        }

        UUri pingUri = LongUriSerializer::deserialize(PING_URI_STRING); //1 pub , many sub 
        UUri pongUri = LongUriSerializer::deserialize(PONG_URI_STRING); //1 sub , many pub

        uint64_t pingTimeMicro;
        std::mutex mutex_;
        size_t maxSubscribers_;
        size_t payloadSize_;
        mutable std::condition_variable cv;
        mutable std::atomic<size_t> responseCounter_;
        mutable std::atomic<uint64_t> latencyTotal_;
        mutable std::atomic<size_t> responseCounterTotal_;
        mutable std::unordered_map<pid_t, LatencyPerPID> processTable_;

        static constexpr auto numOfmessages_ = size_t(2000);
        static constexpr auto numOfWarmupMessages_ = size_t(numOfmessages_ / 2);
};

TEST_F(TestLatencyPing, LatencyTests1Kb1Sub) {
    
    size_t payloadSize[] = {1024, 1024 * 10 , 1024 * 100};
    size_t subscribersCount[] = {1, 10, 20};

    for (size_t size : payloadSize) {
        for (size_t count : subscribersCount) {

            maxSubscribers_ = count;  
            payloadSize_ = size;

            runGTest();      
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

