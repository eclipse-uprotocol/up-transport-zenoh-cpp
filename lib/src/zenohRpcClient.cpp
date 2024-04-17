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

#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-client-zenoh-cpp/session/zenohSessionManager.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-cpp/uri/serializer/MicroUriSerializer.h>
#include <up-client-zenoh-cpp/uri/zenohUri.h>
#include <up-cpp/uri/builder/BuildUResource.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <up-cpp/utils/ThreadPool.h>
#include <up-core-api/ustatus.pb.h>
#include <up-core-api/uattributes.pb.h>
#include <spdlog/spdlog.h>
#include <zenoh.h>
#include <up-client-zenoh-cpp/trace_hook.hpp>

IMPL_TRACEHOOK()

using namespace uprotocol::utransport;
using namespace uprotocol::uuid;
using namespace uprotocol::uri;
using namespace uprotocol::utils;
using namespace uprotocol::rpc;

static std::ostream& operator<<(std::ostream& os, const z_owned_query_t& arg)
{
    os << "z_owned_query_t(" << arg._0 << ')';
    return os;
}

ZenohRpcClient::ZenohRpcClient() noexcept {
    TRACE();
    /* by default initialized to empty strings */
    ZenohSessionManagerConfig config;

    rpcSuccess_.set_code(UCode::UNAVAILABLE);

    if (UCode::OK != ZenohSessionManager::instance().init(config)) {
       spdlog::error("zenohSessionManager::instance().init() failed");
       return;
    }

    if (ZenohSessionManager::instance().getSession().has_value()) {
        session_ = ZenohSessionManager::instance().getSession().value();
    } else {
        spdlog::error("failed to get zenoh session");
        return;
    }

    threadPool_ = make_shared<ThreadPool>(queueSize_, maxNumOfCuncurrentRequests_);
    if (nullptr == threadPool_) {
        spdlog::error("failed to create thread pool");
        return;
    }

    rpcSuccess_.set_code(UCode::OK);
}

ZenohRpcClient::~ZenohRpcClient() noexcept {
    TRACE();
    if (UCode::OK != ZenohSessionManager::instance().term()) {
        spdlog::error("zenohSessionManager::instance().term() failed");
        return;
    }

    spdlog::info("ZenohRpcClient destructor done");
}

std::future<RpcResponse> ZenohRpcClient::invokeMethod(const UUri &topic, 
                                                      const UPayload &payload, 
                                                      const CallOptions &options) noexcept {
    TRACE();
    std::future<RpcResponse> future;

    if (false == isRPCMethod(topic.resource())) {
        spdlog::error("URI is not of RPC type");
        return future;
    }

    if (UPriority::UPRIORITY_CS4 > options.priority()) {
        spdlog::error("Prirority is smaller then UPRIORITY_CS4");
        return future;
    }

    /* TODO : Decide how to handle MUST return ALREADY_EXISTS if the same request already exists 
        (i.e. same UUri and CallOptions). This is to prevent duplicate requests.*/

    return invokeMethodInternal(topic, payload, options);
}

UStatus ZenohRpcClient::invokeMethod(const UUri &topic,
                                     const UPayload &payload,
                                     const CallOptions &options,
                                     const UListener &listener) noexcept {
    TRACE();
    UStatus status;

    status.set_code(UCode::INTERNAL);

    if (false == isRPCMethod(topic.resource())) {
        spdlog::error("URI is not of RPC type");
        return status;
    }

    if (UPriority::UPRIORITY_CS4 > options.priority()) {
        spdlog::error("Prirority is smaller then UPRIORITY_CS4");
        return status;
    }

    auto future = invokeMethodInternal(topic, payload, options, &listener);
    if (false == future.valid()){
        spdlog::error("invokeMethodInternal failed");
        return status;
    }

    status.set_code(UCode::OK);

    return status;
}

std::future<RpcResponse> ZenohRpcClient::invokeMethodInternal(const UUri &topic,
                                                              const UPayload &payload,
                                                              const CallOptions &options,
                                                              const UListener *listener) noexcept {
    TRACE();
    std::future<RpcResponse> future;
    z_owned_bytes_map_t map = z_bytes_map_new();
    z_get_options_t opts = z_get_options_default();
    std::shared_ptr<z_owned_reply_channel_t> channel = nullptr;
    UStatus status;

    status.set_code(UCode::INTERNAL);

    auto key = uprotocol::uri::toZenohKeyString(topic);
    if (key.empty()) {
        spdlog::error("failed to convert topic to zenoh key");
        return future;
    }

    auto uuid = Uuidv8Factory::create();

    auto client_uri = BuildUUri()
                .setAutority(clientAuthority)
                .setEntity(BuildUEntity()
                        .setName("test_rpc.client")
                        .setMajorVersion(1)
                        .setId(1)
                        .build())
                .setResource(BuildUResource()
                        .setRpcRequest("handler", 1)
                        .build())
                .build();

    // {
    //     using namespace std;
    //     cout << "in invokeMethodInternal " << client_uri.DebugString() << endl;
    // }

    // auto builder = UAttributesBuilder::request(topic, topic, options.priority(), options.ttl());
    auto builder = UAttributesBuilder::request(client_uri, topic, options.priority(), options.ttl());

    // builder.setReqid(uuid);
    builder.setTTL(options.ttl());

    UAttributes attributes = builder.build();

    // Serialize UAttributes
    size_t attrSize = attributes.ByteSizeLong();
    std::vector<uint8_t> serializedAttributes(attrSize);
    if (!attributes.SerializeToArray(serializedAttributes.data(), attrSize)) {
        spdlog::error("attributes SerializeToArray failure");
        return future;
    }

    z_bytes_t bytes = {.len = serializedAttributes.size(), .start = serializedAttributes.data()};
    
    z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), bytes);

    channel = std::make_shared<z_owned_reply_channel_t>();
    if (nullptr == channel) {
        spdlog::error("failed to allocate channel");
        return future;
    }

    *channel = zc_reply_fifo_new(16);

    opts.attachment = z_bytes_map_as_attachment(&map);
    opts.timeout_ms = options.ttl();

    if ((0 != payload.size()) && (nullptr != payload.data())) {
        opts.value.payload.len =  payload.size();
        opts.value.payload.start = payload.data();
    } else {
        opts.value.payload.len = 0;
        opts.value.payload.start = nullptr;
    }
    
    cout << "about to call z_get with " << key << " in " << getpid() << endl;
    auto ke = z_keyexpr(key.c_str());
    cout << "key=" << key << " keyexpr=" << get_type(ke) << endl;
    TRACE();
    // if (0 != z_get(z_loan(session_), z_keyexpr(key.c_str()), "", z_move(channel->send), &opts)) {
    if (0 != z_get(z_loan(session_), ke, "", z_move(channel->send), &opts)) {
        spdlog::error("z_get failure");
        return future;
    }
    
    future = threadPool_->submit([channel, listener] { TRACE(); return handleReply(std::move(channel), listener); });

    if (false == future.valid()) {
        spdlog::error("invalid future received");
        return future;
    }

    status.set_code(UCode::OK);

    z_drop(&map);

    TRACE();
    return future;
}

RpcResponse ZenohRpcClient::handleReply(const std::shared_ptr<z_owned_reply_channel_t> &channel,
                                        const UListener *listener) noexcept {
    using namespace std;
    TRACE();
    z_owned_reply_t reply = z_reply_null();
    RpcResponse rpcResponse;

    rpcResponse.status.set_code(UCode::INTERNAL);

    {
        using namespace std;
        cout << "about to call z_call in " << getpid() << endl;
    }
    
    cout << "before" << endl;
    auto before = std::chrono::steady_clock::now();
    TRACE();
    while (z_call(channel->recv, &reply), z_check(reply)) {
        TRACE();
        auto after = std::chrono::steady_clock::now();
        cout << "z_call delay=" << std::chrono::duration_cast<std::chrono::microseconds>(after - before).count() << endl;
        if (!z_reply_is_ok(&reply)) {
            cout << "z_reply_is_ok is false" << endl;
            z_value_t error = z_reply_err(&reply);
            if (memcmp("Timeout", error.payload.start, error.payload.len) == 0) {
                spdlog::error("Timeout received while waiting for response");
                rpcResponse.status.set_code(UCode::DEADLINE_EXCEEDED);
            } else {
                spdlog::error("Error received while waiting for response");
            }

            break;
        }
        cout << "z_reply_is_ok is true" << endl;

        TRACE();
        z_sample_t sample = z_reply_ok(&reply);
        TRACE();

        if (!z_check(sample.attachment)) {
            spdlog::error("No attachment found in the reply");
            break;
        }       

        TRACE();
        z_bytes_t serializedAttributes = z_attachment_get(sample.attachment, z_bytes_new("attributes"));
        TRACE();
        
        if ((0 == serializedAttributes.len) || (nullptr == serializedAttributes.start)) {
            spdlog::error("Serialized attributes not found in the attachment");
            break;
        }

        uprotocol::v1::UAttributes attributes;
        if (!attributes.ParseFromArray(serializedAttributes.start, serializedAttributes.len)) {
            spdlog::error("ParseFromArray failure");
            break;
        }

        auto payload = UPayload(sample.payload.start, sample.payload.len, UPayloadType::VALUE);

        rpcResponse.message.setPayload(payload);
        rpcResponse.message.setAttributes(attributes);
        rpcResponse.status.set_code(UCode::OK);

        TRACE();
        z_drop(z_move(reply));
    }

    z_drop(channel.get());

    /* TODO - how to send an error to the user*/
    if ((nullptr != listener) && (UCode::OK == rpcResponse.status.code())) {
        listener->onReceive(rpcResponse.message);
    }

    return rpcResponse;
}