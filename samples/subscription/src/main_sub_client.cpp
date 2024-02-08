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

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::uSubscription;

bool gTerminate = false; 

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true; 
    }
}

class TimeListener : public UListener {
    
    UStatus onReceive(const UUri &uri, 
                      const UPayload &payload, 
                      const UAttributes &attributes) const {
        UStatus status;

        status.set_code(UCode::OK);

        return status;
    }
};

CreateTopicRequest buildCreateTopicRequest(UUri &uri) {

    CreateTopicRequest request;

    auto topic = request.mutable_topic();

    topic = &uri;

    return std::move(request);
}

DeprecateTopicRequest buildDeprecateTopicRequest(UUri &uri) {

    DeprecateTopicRequest request;

    auto topic = request.mutable_topic();

    topic = &uri;
    
    return std::move(request);
}

UnsubscribeRequest buildUnsubscribeRequest(UUri &uri) {

    UnsubscribeRequest request;

    auto topic = request.mutable_topic();

    topic = &uri;

    return std::move(request);
}

SubscriptionRequest buildSubscriptionRequest(UUri &uri) {

    SubscriptionRequest request;

    auto topic = request.mutable_topic();

    topic->CopyFrom(uri);

    return std::move(request);
}

int main(int argc, char **argv) {

    TimeListener listener;
    std::string userInput;

    signal(SIGINT, signalHandler);

    if (1 < argc) {
        if (0 == strcmp("-d", argv[1])) {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    ZenohUTransport *transport = &ZenohUTransport::instance();
    uSubscriptionClient *uSubClient = &uSubscriptionClient::instance();

    if (UCode::OK != transport->init().code()) {
        spdlog::error("ZenohUTransport::instance().init failed");
        return -1;
    }

    if (UCode::OK != uSubClient->init().code()) {
        spdlog::error("uSubscriptionClient::instance().init() failed");
        return -1;
    }

    auto realUri = LongUriSerializer::deserialize("/real.app/1/milliseconds");
    auto demoUri = LongUriSerializer::deserialize("/demo.app/1/milliseconds");

    auto req1 = buildCreateTopicRequest(realUri);
    auto req2 = buildCreateTopicRequest(demoUri);
    auto req3 = buildSubscriptionRequest(realUri);
    auto req5 = buildSubscriptionRequest(demoUri);

    auto req4 = buildUnsubscribeRequest(realUri);


    // uSubClient->createTopic(req1);
    // uSubClient->createTopic(req2);
    uSubClient->subscribe(req3);
    uSubClient->subscribe(req5);

    if (UCode::OK != uSubClient->term().code()) {
        spdlog::error("uSubscriptionClient::instance().term() failed");
        return -1;
    }

    if (UCode::OK != transport->term().code()) {
        spdlog::error("ZenohUTransport::instance().term() failed");
        return -1;
    }

    return 0;
}