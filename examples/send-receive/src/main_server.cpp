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
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <up-core-api/ustatus.pb.h>
#include <up-core-api/uri.pb.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;

bool gTerminate = false; 

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true; 
    }
}

static uint8_t* getTime() {

    auto currentTime = std::chrono::system_clock::now();
    auto duration = currentTime.time_since_epoch();
    
    auto timeMilli = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    static uint8_t buf[8];

    memcpy(buf, &timeMilli, sizeof(timeMilli));

    return buf;
}

static uint8_t* getRandom() {
    int32_t val = rand();

    static uint8_t buf[4];

    memcpy(buf, &val, sizeof(val));

    return buf;
}

static uint8_t* getCounter() {
    static uint8_t counter = 0;

    ++counter;

    return &counter;
}

UCode sendMessage(UUri &uri, 
                  uint8_t *buffer,
                  size_t size) {

    auto uuid = Uuidv8Factory::create();

    UAttributesBuilder builder(uuid, UMessageType::PUBLISH, UPriority::STANDARD);

    UAttributes attributes = builder.build();

    UPayload payload(buffer, size, UPayloadType::VALUE);
   
    if (UCode::OK != ZenohUTransport::instance().send(uri, payload, attributes).code()) {
        spdlog::error("ZenohUTransport::instance().send failed");
        return UCode::UNAVAILABLE;
    }

    return UCode::OK;
}

int main(int argc, char **argv) {

    signal(SIGINT, signalHandler);

    if (1 < argc) {
        if (0 == strcmp("-d", argv[1])) {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    if (UCode::OK != ZenohUTransport::instance().init().code()) {
        spdlog::error("ZenohUTransport::instance().init failed");
        return -1;
    }
    
    //  /entity/
    auto timeUri = LongUriSerializer::deserialize("/test.app/1/milliseconds");
    auto randomUri = LongUriSerializer::deserialize("/test.app/1/32bit"); 
    auto counterUri = LongUriSerializer::deserialize("/test.app/1/counter");

    while (!gTerminate) {

         if (UCode::OK != sendMessage(timeUri, getTime(), 8)) {
            spdlog::error("sendMessage failed");
            break;
         }

         if (UCode::OK != sendMessage(randomUri, getRandom(), 4)) {
            spdlog::error("sendMessage failed");
            break;
         }

         if (UCode::OK != sendMessage(counterUri, getCounter(), 1)) {
            spdlog::error("sendMessage failed");
            break;
         }

         sleep(1);
    }

    if (UCode::OK != ZenohUTransport::instance().term().code()) {
        spdlog::error("ZenohUTransport::instance().term() failed");
        return -1;
    }

    return 0;
}