/*
 * Copyright (c) 2023 General Motors GTO LLC
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
 * SPDX-FileCopyrightText: 2023 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csignal>
#include <spdlog/spdlog.h>
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uri;
using namespace uprotocol::rpc;

UUri rpcUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.handler");
UUri rpcNoServerUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.noServer");

class RpcServer : public UListener {

     public:
        UStatus onReceive(const UUri& uri,
                          const UPayload& payload,
                          const UAttributes& attributes) const override {
            
            (void) uri;
            (void) payload;
            (void) attributes;

            UStatus status;

            return status;
        }

         UStatus onReceive(UMessage &message) const override {

            (void) message;
            
            UStatus status;

            return status;
            
            // std::cout << "OnReceive" << std::endl;
            //       /* Construct response payload with the current time */
            // auto currentTime = std::chrono::system_clock::now();
            // auto duration = currentTime.time_since_epoch();
            // uint64_t currentTimeMilli = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

            // UPayload responsePayload(reinterpret_cast<const uint8_t*>(&currentTimeMilli), sizeof(currentTimeMilli), UPayloadType::VALUE);

            // /* Build response attributes - the same UUID should be used to send the response 
            //  * it is also possible to send the response outside of the callback context */
            // UAttributesBuilder builder(message.attributes().id(), UMessageType::UMESSAGE_TYPE_RESPONSE, UPriority::UPRIORITY_CS0);
            // UAttributes responseAttributes = builder.build();

            // auto rpcUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.milliseconds");
            // /* Send the response */
            // return ZenohUTransport::instance().send(rpcUri, responsePayload, responseAttributes);
        }
};

class TestRPcClient : public ::testing::Test {

    public:
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }

            if (UCode::OK != ZenohRpcClient::instance().init().code()) {
                spdlog::error("ZenohRpcClient::instance().init failed");
                return;
            }

            ZenohUTransport::instance().registerListener(rpcUri, TestRPcClient::rpcListener);

        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }

            if (UCode::OK != ZenohRpcClient::instance().term().code()) {
                spdlog::error("ZenohRpcClient::instance().term() failed");
                return;
            }
        }
    
        static RpcServer rpcListener;

};

RpcServer TestRPcClient::rpcListener;

/* Deprecate non existing topic */
TEST_F(TestRPcClient, InvokeMethodWithoutServer) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcNoServerUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

/* Deprecate non existing topic */
TEST_F(TestRPcClient, invokeMethodNoResponse) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

/* Deprecate non existing topic */
TEST_F(TestRPcClient, maxSimultaneousRequests) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(5000);

    size_t numRequestsUntilQueueIsFull = ZenohRpcClient::instance().getMaxConcurrentRequests() + ZenohRpcClient::instance().getQueueSize();

    for (size_t i = 0; i < (numRequestsUntilQueueIsFull + 1) ; ++i) {
        std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

        if (i < numRequestsUntilQueueIsFull) {
            EXPECT_EQ(future.valid(), true);
        } else {
            EXPECT_EQ(future.valid(), false);
        }
    }
}

//priority is less the CS4
//response is 0
//response is non zero 

//send response on the same UUID twice
//request payload is 0
//request payload is non zero 
//send response on expired TTL 

//unsubscribe from non existane topic 
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}