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
#include <uprotocol-cpp-ulink-zenoh/transport/zenohUTransport.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;

using RpcRequest = std::pair<std::unique_ptr<UUri>, std::unique_ptr<UAttributes>>;

bool gTerminate = false; 

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Ctrl+C received. Exiting..." << std::endl;
        gTerminate = true; 
    }
}

class RpcListener : public UListener {

    UStatus onReceive(const UUri &uri, 
                      const UPayload &payload, 
                      const UAttributes &attributes) const {

        
        auto currentTime = std::chrono::system_clock::now();
        auto duration = currentTime.time_since_epoch();
        
        auto timeMilli = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        static uint8_t buf[8];

        memcpy(buf, &timeMilli, sizeof(timeMilli));

        UPayload response(buf, sizeof(buf), UPayloadType::VALUE);

        auto type = UMessageType::RESPONSE;
        auto priority = UPriority::STANDARD;

        UAttributesBuilder builder(attributes.id(), type, priority);

        ZenohUTransport::instance().send(uri, response, builder.build());

        UStatus status;
        status.set_code(UCode::OK);

        return status;
    }
    
};

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);

    RpcListener rpcListener;

    if (1 < argc) {
        if (0 == strcmp("-d", argv[1])) {
            spdlog::set_level(spdlog::level::debug);
        }
    }

    if (UCode::OK != ZenohUTransport::instance().init().code()) {
        spdlog::error("ZenohRpcServer::instance().init failed");
        return -1;
    }

    auto rpcUri = UUri(UAuthority::local(), UEntity::longFormat("test_rpc.app"), UResource::forRpcRequest("getTime"));;

    if (UCode::OK != ZenohUTransport::instance().registerListener(rpcUri, rpcListener).code()) {
        spdlog::error("ZenohRpcServer::instance().registerListener failed");
        return -1;
    }

    while (!gTerminate) {
        sleep(1);
    }

    if (UCode::OK != ZenohUTransport::instance().unregisterListener(rpcUri, rpcListener).code()) {
        spdlog::error("ZenohRpcServer::instance().unregisterListener failed");
        return -1;
    }

    if (UCode::OK != ZenohUTransport::instance().term().code()) {
        spdlog::error("ZenohUTransport::instance().term failed");
        return -1;
    }

    return 0;
}
