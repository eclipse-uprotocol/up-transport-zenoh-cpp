
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
#include <iostream>
#include <spdlog/spdlog.h>
#include <unistd.h> // For sleep
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::v1;
using namespace uprotocol::uuid;

const std::string PING_URI_STRING = "/latency.app/1/ping";
const std::string PONG_URI_STRING = "/latency.app/1/32bit";

bool gTerminate = false;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true;
    }
}

class CustomListener : public UListener {

    public:

        CustomListener() {
            auto pingUUid = Uuidv8Factory::create();
            UAttributesBuilder builder(pingUUid, UMessageType::UMESSAGE_TYPE_PUBLISH, UPriority::UPRIORITY_CS0);
            responseAttributes = builder.build();
        }

        UStatus onReceive(UMessage &message) const override {
            
            
            (void)message;        
         
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts); // Get current time
            // Convert seconds to microseconds and add nanoseconds converted to microseconds
            uint64_t currentTime = static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
            pid_t pid = getpid();

            uint8_t respBuffer[sizeof(pid_t) + sizeof(currentTime)];

            memcpy(respBuffer, &currentTime, sizeof(currentTime));
            memcpy(respBuffer + sizeof(currentTime), &pid , sizeof(pid));

            UPayload payload(respBuffer, sizeof(respBuffer), UPayloadType::VALUE);

            /* Send the response */
            return ZenohUTransport::instance().send(pongUri, payload, responseAttributes);
        }

        UUri pongUri = LongUriSerializer::deserialize(PONG_URI_STRING);
        UAttributes responseAttributes;
        uint8_t buffer[1];
};

/* The sample sub applications demonstrates how to consume data using uTransport -
 * There are three topics that are received - random number, current time and a counter */
int main(int argc, 
         char** argv) {

    (void)argc;
    (void)argv;
    
    signal(SIGINT, signalHandler);

    UStatus status;
    CustomListener listener;

    ZenohUTransport *transport = &ZenohUTransport::instance();
    
    /* init zenoh utransport */
    status = transport->init();
    if (UCode::OK != status.code()){
        spdlog::error("ZenohUTransport init failed");
        return -1;
    }

    UUri pingUri = LongUriSerializer::deserialize(PING_URI_STRING); 
       
    status = transport->registerListener(pingUri, listener);
    if (UCode::OK != status.code()){
        spdlog::error("registerListener failed");
        return -1;
    }

    while (!gTerminate) {
        sleep(1);
    }

    status = transport->unregisterListener(pingUri, listener);
    if (UCode::OK != status.code()){
        spdlog::error("unregisterListener failed");
        return -1;
    }
    
    /* term zenoh utransport */
    status = transport->term();
    if (UCode::OK != status.code()) {
        spdlog::error("ZenohUTransport term failed");
        return -1;
    }

    return 0;
}
