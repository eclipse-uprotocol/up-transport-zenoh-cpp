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

#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <chrono>
#include <csignal>
#include <unistd.h>

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

UPayload sendRPC(UUri &uri) {

    auto uuid = Uuidv8Factory::create(); 
    auto type = UMessageType::REQUEST;
    auto priority = UPriority::STANDARD;

    UAttributesBuilder builder(uuid, type, priority);

    UAttributes attributes = builder.build();

    uint8_t buffer[1];

    UPayload payload(buffer, sizeof(buffer), UPayloadType::VALUE);

    std::future<UPayload> result = ZenohRpcClient::instance().invokeMethod(uri, payload, attributes);

    if (false == result.valid()) {
        spdlog::error("future is invalid");
        return UPayload(nullptr, 0, UPayloadType::UNDEFINED);   
    }

    result.wait();

    return result.get();
}

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);

    if (1 < argc) {
        if (0 == strcmp("-d", argv[1])) {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    if (UCode::OK != ZenohRpcClient::instance().init().code())
    {
        spdlog::error("ZenohRpcClient::instance().init failed");
        return -1;
    }

    auto rpcUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.milliseconds");

    while (!gTerminate) {

        auto response = sendRPC(rpcUri);

        uint64_t milliseconds;

        if (nullptr != response.data()) {

            memcpy(&milliseconds, response.data(), response.size());

            spdlog::info("received = {}", milliseconds);
        }
        sleep(1);
    }

    if (UCode::OK != ZenohRpcClient::instance().term().code()) {
        spdlog::error("ZenohRpcClient::instance().term() failed");
        return -1;
    }

    return 0;
}
