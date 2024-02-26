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

#ifndef _MESSAGEPARSER_H_
#define _MESSAGEPARSER_H_

#include <unordered_map>
#include <vector>
#include <up-client-zenoh-cpp/message/messageCommon.h>
#include <up-cpp/utils/base64.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-cpp/transport/datamodel/UAttributes.h>
#include <up-core-api/uri.pb.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::utils;

struct TLV {
    Tag type;
    size_t length;
    std::vector<uint8_t> value;
};

/**
* parse the message received over the transport
*/
class MessageParser
{
    public:

        static std::optional<std::unordered_map<Tag,TLV>> getAllTlv(const uint8_t *data, 
                                                                    size_t size) noexcept;

        static std::optional<UAttributes> getAttributes(std::unordered_map<Tag,TLV> &map) noexcept;

        static std::optional<UPayload> getPayload(std::unordered_map<Tag,TLV> &map) noexcept;       

    private:

        static bool isValidTag(Tag value);
};

#endif /*_MESSAGEPARSER_H_ */