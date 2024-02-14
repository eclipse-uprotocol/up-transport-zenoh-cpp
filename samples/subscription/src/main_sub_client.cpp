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
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/usubscription/uSubscriptionClient.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <ustatus.pb.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::uSubscription;

class uSubTestSuite : public ::testing::Test {

    public:

        template <typename T>
        T buildTopicRequest(UUri &uri) {

            T request;

            auto topic = request.mutable_topic();

            topic->CopyFrom(uri);

            return std::move(request);
        }

        template <typename T>
        T buildSubscriptionRequest(UUri &topicToSubscribe, 
                                   UUri &appIdentity) {
            T request;

            // topic to subscribe too 
            auto topic = request.mutable_topic();
            auto uri = request.mutable_subscriber()->mutable_uri();

            topic->CopyFrom(topicToSubscribe);
            uri->CopyFrom(appIdentity);

            return std::move(request);
        }

    public:
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }

            if (UCode::OK != uSubscriptionClient::instance().init().code()) {
                spdlog::error("uSubscriptionClient::instance().init() failed");
                return;
            }
        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {
            if (UCode::OK != uSubscriptionClient::instance().term().code()) {
                spdlog::error("uSubscriptionClient::instance().term() failed");
                return;
            }

            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }
        }

        UUri topic = LongUriSerializer::deserialize("/producer.app/1/milliseconds");
        UUri appIdentityV1 = LongUriSerializer::deserialize("/consumer.app/1/");
        UUri appIdentityV2 = LongUriSerializer::deserialize("/consumer.app/2/");
};


// /* Calling subscribe before topic is created*/
TEST_F(uSubTestSuite, subscribeBeforeCreateTopic) {
    
    SubscriptionRequest request = buildSubscriptionRequest<SubscriptionRequest>(topic, appIdentityV1);
    
    auto response = uSubscriptionClient::instance().subscribe(request);

    EXPECT_NE(response, std::nullopt);

    EXPECT_NE(response.value().status().state(), SubscriptionStatus_State::SubscriptionStatus_State_SUBSCRIBED);
}

/* Calling subscribe after topic is created*/
TEST_F(uSubTestSuite, subscribe) {
    
    CreateTopicRequest topicRequest = buildTopicRequest<CreateTopicRequest>(topic);
    SubscriptionRequest subRequest = buildSubscriptionRequest<SubscriptionRequest>(topic, appIdentityV1);
    
    UStatus createResponse = uSubscriptionClient::instance().createTopic(topicRequest);

    EXPECT_EQ(createResponse.code(), UCode::OK);

    auto response = uSubscriptionClient::instance().subscribe(subRequest);

    EXPECT_NE(response, std::nullopt);

    EXPECT_EQ(response.value().status().state(), SubscriptionStatus_State::SubscriptionStatus_State_SUBSCRIBED);
}

/* Calling subscribe after topic is created*/
TEST_F(uSubTestSuite, deprecateTopicAndSubscribe) {
    
    DeprecateTopicRequest topicRequest = buildTopicRequest<DeprecateTopicRequest>(topic);
    SubscriptionRequest subRequest = buildSubscriptionRequest<SubscriptionRequest>(topic, appIdentityV2);
    
    auto deprecateResponse = uSubscriptionClient::instance().deprecateTopic(topicRequest);

    EXPECT_EQ(deprecateResponse.code(), UCode::OK);

    auto response = uSubscriptionClient::instance().subscribe(subRequest);

    EXPECT_NE(response, std::nullopt);

    EXPECT_EQ(response.value().status().state(), SubscriptionStatus_State::SubscriptionStatus_State_UNSUBSCRIBED);
}

/* Calling subscribe after topic is created*/
TEST_F(uSubTestSuite, unSubscribe) {
    
    UnsubscribeRequest subRequest = buildSubscriptionRequest<UnsubscribeRequest>(topic, appIdentityV1);
    
    auto response = uSubscriptionClient::instance().unSubscribe(subRequest);

    EXPECT_EQ(response.code(), UCode::OK);
}

//unsubscribe from non existane topic 

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
