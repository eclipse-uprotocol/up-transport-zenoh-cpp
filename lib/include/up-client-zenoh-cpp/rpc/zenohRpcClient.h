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

#ifndef _ZENOH_RPC_CLIENT_H_
#define _ZENOH_RPC_CLIENT_H_

#include <condition_variable>
#include <up-cpp/rpc/RpcClient.h>
#include <up-cpp/utils/ThreadPool.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-core-api/ustatus.pb.h>
#include <up-core-api/uri.pb.h>
#include <zenoh.h>

namespace uprotocol::rpc {

    struct ZenohRpcClientConfig
    {
        size_t maxQueueSize;
        size_t maxConcurrentRequests;
    };

    public:
        ZenohRpcClient(const ZenohRpcClient&) = delete;
        ZenohRpcClient& operator=(const ZenohRpcClient&) = delete;

        /**
        * Support for RPC method invocation.
        * @param topic topic of the method to be invoked (i.e. the name of the API we are calling).
        * @param payload The request message to be sent to the server.
        * @param attributes Metadata for the method invocation (i.e. priority, timeout, etc.)
        * @return Returns the CompletableFuture with the result or exception.
        */
        std::future<UPayload> invokeMethod(const UUri &uri, 
                                           const UPayload &payload, 
                                           const UAttributes &attributes) noexcept;
    protected:
        /* Initialization success/failure status */
        UStatus rpcSuccess_;

        ZenohRpcClient() noexcept;
        ~ZenohRpcClient() noexcept;

    private:
        static UPayload handleReply(z_owned_reply_channel_t *channel);

        /* zenoh session handle*/
        z_owned_session_t session_;
        
        std::shared_ptr<ThreadPool> threadPool_;

            /**
             * get the number of max concurrent request 
             * @return number of concurrent requests
            */
            size_t getMaxConcurrentRequests() {
                return maxNumOfCuncurrentRequests_;
            }
            
            /**
             * get queue size
             * @return queue size
            */
            size_t getQueueSize() {
                return queueSize_;
            }

        private:

           ZenohRpcClient() {}

           std::future<uprotocol::rpc::RpcResponse> invokeMethodInternal(const uprotocol::v1::UUri &topic,
                                                                         const uprotocol::utransport::UPayload &payload,
                                                                         const uprotocol::v1::CallOptions &options,
                                                                         const uprotocol::utransport::UListener *callback = nullptr) noexcept;

            static uprotocol::rpc::RpcResponse handleReply(z_owned_reply_channel_t *channel, 
                                                           const uprotocol::utransport::UListener *callback = nullptr) noexcept;
                        
            /* zenoh session handle*/
            z_owned_session_t session_;
            /* how many times uTransport was initialized*/
            atomic_uint32_t refCount_ = 0;
            
            std::mutex mutex_;
            std::shared_ptr<uprotocol::utils::ThreadPool> threadPool_;

            static constexpr auto requestTimeoutMs_ = 5000;
            static constexpr auto queueSizeDefault_ = size_t(20);
            static constexpr auto maxNumOfCuncurrentRequestsDefault_ = size_t(2);

            size_t queueSize_ = queueSizeDefault_;
            size_t maxNumOfCuncurrentRequests_ = maxNumOfCuncurrentRequestsDefault_;
    };
}

#endif /*_ZENOH_RPC_CLIENT_H_*/
