
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
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::uri;

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

        uint64_t timeInMilliseconds;
      
        memcpy(&timeInMilliseconds, payload.data(), payload.size());

        spdlog::info("time = {}", timeInMilliseconds);

        UStatus status;

        status.set_code(UCode::OK);

        return status;
    }
};

class RandomListener : public UListener {

    UStatus onReceive(const UUri &uri, 
                      const UPayload &payload, 
                      const UAttributes &attributes) const {

        uint32_t random;

        memcpy(&random, payload.data(), payload.size());

        spdlog::info("random = {}", random);

        UStatus status;
        
        status.set_code(UCode::OK);

        return status;
    }
};

class CounterListener : public UListener {

    UStatus onReceive(const UUri &uri, 
                      const UPayload &payload, 
                      const UAttributes &attributes) const {

        uint8_t counter;

        memcpy(&counter, payload.data(), payload.size());

        spdlog::info("counter = {}", counter);

        UStatus status;
        
        status.set_code(UCode::OK);

        return status;
    }
};

int main(int argc, char **argv) {

    signal(SIGINT, signalHandler);

    TimeListener timeListener;
    RandomListener randomListener;
    CounterListener counterListener;

    if (1 < argc) {
        if (0 == strcmp("-d", argv[1])) {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    if (UCode::OK != ZenohUTransport::instance().init().code()) {
        spdlog::error("ZenohUTransport::instance().init failed");
        return -1;
    }

    auto timeUri = LongUriSerializer::deserialize("/test.app/1/milliseconds");
    auto randomUri = LongUriSerializer::deserialize("/test.app/1/32bit"); 
    auto counterUri = LongUriSerializer::deserialize("/test.app/1/counter");

    if (UCode::OK != ZenohUTransport::instance().registerListener(timeUri, timeListener).code()) {

        spdlog::error("ZenohUTransport::instance().registerListener failed");
        return -1;
    }
       
    if (UCode::OK != ZenohUTransport::instance().registerListener(randomUri, randomListener).code()) {

        spdlog::error("ZenohUTransport::instance().registerListener failed");
        return -1;
    }

    if (UCode::OK != ZenohUTransport::instance().registerListener(counterUri, counterListener).code()) {

        spdlog::error("ZenohUTransport::instance().registerListener failed");
        return -1;
    }

    while(!gTerminate) {
        sleep(1);
    }
   
    if (UCode::OK != ZenohUTransport::instance().unregisterListener(timeUri, timeListener).code()) {

        spdlog::error("ZenohUTransport::instance().unregisterListener failed");
        return -1;
    }

    if (UCode::OK != ZenohUTransport::instance().unregisterListener(randomUri, randomListener).code()) {

        spdlog::error("ZenohUTransport::instance().unregisterListener failed");
        return -1;
    }

    if (UCode::OK != ZenohUTransport::instance().unregisterListener(counterUri, counterListener).code()) {

        spdlog::error("ZenohUTransport::instance().unregisterListener failed");
        return -1;
    }

    if (UCode::OK != ZenohUTransport::instance().term().code()) {
        spdlog::error("ZenohUTransport::instance().term failed");
        return -1;
    }

    return 0;
}