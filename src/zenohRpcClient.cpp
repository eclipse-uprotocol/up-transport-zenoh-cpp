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

#include <uprotocol-cpp-ulink-zenoh/rpc/zenohRpcClient.h>
#include <uprotocol-cpp-ulink-zenoh/message/messageBuilder.h>
#include <uprotocol-cpp-ulink-zenoh/message/messageParser.h>
#include <uprotocol-cpp-ulink-zenoh/session/zenohSessionManager.h>
#include <uprotocol-cpp/uuid/serializer/UuidSerializer.h>
#include <uprotocol-cpp/uri/serializer/LongUriSerializer.h>
#include <uprotocol-cpp/transport/datamodel/UPayload.h>
#include <uprotocol-cpp/transport/datamodel/UAttributes.h>
#include <src/main/proto/ustatus.pb.h>
#include <spdlog/spdlog.h>
#include <zenoh.h>
#include <uuid/uuid.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uuid;
using namespace uprotocol::uri;
using namespace uprotocol::v1;

ZenohRpcClient& ZenohRpcClient::instance(void) noexcept {
    
    static ZenohRpcClient rpcClient;

    return rpcClient;
}

UStatus ZenohRpcClient::init() noexcept {

    UStatus status;

    if (0 == refCount_) {

        std::lock_guard<std::mutex> lock(mutex_);

        if (0 == refCount_) {
            /* by default initialized to empty strings */
            ZenohSessionManagerConfig config;

            if (UCode::OK != ZenohSessionManager::instance().init(config)) {
                spdlog::error("zenohSessionManager::instance().init() failed");
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }

            if (ZenohSessionManager::instance().getSession().has_value()) {
                session_ = ZenohSessionManager::instance().getSession().value();
            } else {
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }

            threadPool_ = make_shared<ThreadPool>(threadPoolSize_);
            if (nullptr == threadPool_) {
                spdlog::error("failed to create thread pool");
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }

            threadPool_->init();

        }
        refCount_.fetch_add(1);

    } else {
        refCount_.fetch_add(1);
    }

    status.set_code(UCode::OK);

    return status;
}

UStatus ZenohRpcClient::term() noexcept {

    UStatus status;
    
    std::lock_guard<std::mutex> lock(mutex_);

    refCount_.fetch_sub(1);

    if (0 == refCount_) {

        threadPool_->term();

        if (UCode::OK != ZenohSessionManager::instance().term()) {
            spdlog::error("zenohSessionManager::instance().term() failed");
            status.set_code(UCode::UNAVAILABLE);
            return status;
        }
    }

    status.set_code(UCode::OK);

    return status;
} 

std::future<UPayload> ZenohRpcClient::invokeMethod(const UUri &uri, 
                                                   const UPayload &payload, 
                                                   const UAttributes &attributes) noexcept {
    std::future<UPayload> future;

    if (0 == refCount_) {
        spdlog::error("ZenohRpcClient is not initialized");
        return std::move(future);
    }
    
    if (UMessageType::REQUEST != attributes.type()) {
        spdlog::error("Wrong message type = {}", UMessageTypeToString(attributes.type()).value());
        return std::move(future);
    }
    
    auto uriHash = std::hash<std::string>{}(LongUriSerializer::serialize(uri));

    if (UMessageType::REQUEST != attributes.type()) {
        spdlog::error("Wrong message type = {}", UMessageTypeToString(attributes.type()).value());
        return std::move(future);
    }
  
    auto message = MessageBuilder::build(attributes, payload);
    if (0 == message.size()) {
        spdlog::error("MessageBuilder failure");
        return std::move(future);
    }

    z_owned_reply_channel_t *channel = new z_owned_reply_channel_t;

   *channel = zc_reply_fifo_new(16);

    z_get_options_t opts = z_get_options_default();

    opts.timeout_ms = requestTimeoutMs_;

    opts.value.payload = (z_bytes_t){.len =  message.size(), .start = (uint8_t *)message.data()};

    auto uuidStr = UuidSerializer::serializeToString(attributes.id());

    if (0 != z_get(z_loan(session_), z_keyexpr(std::to_string(uriHash).c_str()), "", z_move(channel->send), &opts)) {
        spdlog::error("z_get failure");
        return std::move(future);
    }  
    
    future = threadPool_->submit(handleReply, channel);

    if (false == future.valid()) {
        spdlog::error("failed to invoke method");
    }
   
    return future; 
}

UPayload ZenohRpcClient::handleReply(z_owned_reply_channel_t *channel) {

    z_owned_reply_t reply = z_reply_null();
    UPayload response (nullptr, 0, UPayloadType::VALUE);
        
    for (z_call(channel->recv, &reply); z_check(reply); z_call(channel->recv, &reply)) {

        if (z_reply_is_ok(&reply)) {
            z_sample_t sample = z_reply_ok(&reply);
            z_owned_str_t keystr = z_keyexpr_to_string(sample.keyexpr);

            z_drop(z_move(keystr));

            auto tlvVector = MessageParser::getAllTlv(sample.payload.start, sample.payload.len);
     
            if (false == tlvVector.has_value()) {

                spdlog::error("getAllTlv failure");
                return response;
            }

            auto respOpt = MessageParser::getPayload(tlvVector.value());    
            if (false == respOpt.has_value()) {
                spdlog::error("getPayload failure");
                return response;
            }

            response = std::move(respOpt.value());
        } else {

            spdlog::error("error received");
            z_drop(z_move(reply));
            z_drop((channel)); 
        }
    }

    z_drop(z_move(reply));
    z_drop((channel));    

    delete channel;

    return response;
}