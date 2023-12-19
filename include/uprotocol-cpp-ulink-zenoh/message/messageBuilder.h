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

#ifndef _MESSAGE_BUILDER_H_
#define _MESSAGE_BUILDER_H_

#include <cstdint>
#include <cstring>
#include <uprotocol-cpp/tools/base64.h>
#include <uprotocol-cpp/uri/datamodel/UUri.h>
#include <uprotocol-cpp/transport/datamodel/UPayload.h>
#include <uprotocol-cpp/transport/datamodel/UAttributes.h>
#include <uprotocol-cpp-ulink-zenoh/message/messageCommon.h>
#include <src/main/proto/ustatus.pb.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uri;

class MessageBuilder 
{
    public:

        /**
        * build the message to be send over the transport
        * @param attributes Metadata for the method invocation (i.e. priority, timeout, etc.)
        * @param payload The request message to be sent to the server.
        * @return returns a buffer ready to be sent
        */
        static std::vector<uint8_t> build(const UAttributes &attributes, 
                                          const UPayload &payload) noexcept;
    private:

        static size_t calculateSize(const UAttributes &attributes, 
                                    const UPayload &payload) noexcept;
        
        template <typename T>
        static void updateSize(const T &value, 
                               size_t &msgSize) noexcept;

        static void updateSize(size_t size, 
                               size_t &msgSize) noexcept;

        static size_t addTag(std::vector<uint8_t>& buffer,
                             Tag tag, 
                             const uint8_t* data, 
                             size_t size, 
                             size_t pos);
        
        template <typename T>
        static size_t addTag(std::vector<uint8_t>& buffer, 
                             Tag tag, 
                             const T& value, 
                             size_t pos);
};

#endif /* _MESSAGE_BUILDER_H_ */