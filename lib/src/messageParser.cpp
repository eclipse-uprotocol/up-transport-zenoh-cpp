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

#include <up-client-zenoh-cpp/message/messageParser.h>
#include <up-cpp/utils/base64.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-cpp/transport/datamodel/UAttributes.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <spdlog/spdlog.h>
#include <up-core-api/uri.pb.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uuid;
using namespace uprotocol::utils;

std::optional<std::unordered_map<Tag,TLV>> MessageParser::getAllTlv(const uint8_t *data, 
                                                                    size_t size) noexcept {
    if (nullptr == data) {
        return std::nullopt;
    }

    if (0 == size) {
        return std::nullopt;
    }

    size_t pos = 0;
    std::unordered_map<Tag, TLV> umap; 

    while (size > pos)
    {
        TLV tlv;   

        tlv.type = static_cast<Tag>(data[pos]);

        if (false == isValidTag(tlv.type)) {
            return std::nullopt;
        }
        
        pos += 1;

        if (size < pos) {
            return std::nullopt;
        }

        std::memcpy(&tlv.length, data + pos,  sizeof(tlv.length));

        pos += sizeof(tlv.length);
        
        if (size < pos) {
            return std::nullopt;
        }

        tlv.value.insert(tlv.value.end(), data + pos, data + (pos + tlv.length));

        pos += tlv.length;

        umap[tlv.type] = tlv;
    }

    return umap;
}

std::optional<UAttributes> MessageParser::getAttributes(std::unordered_map<Tag,TLV> &map) noexcept {

    UMessageType type;
    UPriority priority;

    if (map.find(Tag::ID) == map.end()) {
        spdlog::error("ID tag not found");
        return std::nullopt;
    }

    if (map.find(Tag::TYPE) == map.end()) {
        spdlog::error("TYPE tag not found");
        return std::nullopt;
    }

    if (map.find(Tag::PRIORITY) == map.end()) {
        spdlog::error("PRIORITY tag not found");
        return std::nullopt;
    }

    std::vector<unsigned char> idVector(map[Tag::ID].value.data(), map[Tag::ID].value.data() + map[Tag::ID].value.size());

    UUID id = UuidSerializer::deserializeFromBytes(idVector);

    std::memcpy(&type, map[Tag::TYPE].value.data(), map[Tag::TYPE].length);

    std::memcpy(&priority, map[Tag::PRIORITY].value.data(), map[Tag::PRIORITY].length);

    UAttributesBuilder attributesBuilder(id, type, priority);

    if (map.find(Tag::TTL) != map.end()) {
        int32_t value = *reinterpret_cast<int32_t*>(map[Tag::ID].value.data());
        attributesBuilder.withTtl(value);
    }

    if (map.find(Tag::PLEVEL) != map.end()) {
        int32_t value = *reinterpret_cast<int32_t*>(map[Tag::PLEVEL].value.data());
        attributesBuilder.withPermissionLevel(value);
    }

    if (map.find(Tag::COMMSTATUS) != map.end()) {
        int32_t value = *reinterpret_cast<int32_t*>(map[Tag::COMMSTATUS].value.data());
        attributesBuilder.withCommStatus(value);
    }

    /* TODO add support for remaining tags*/

    return attributesBuilder.build();
}

std::optional<UPayload> MessageParser::getPayload(std::unordered_map<Tag,TLV> &map) noexcept
{
    if (map.find(Tag::PAYLOAD) == map.end()) {
        spdlog::error("PAYLOAD tag not found");
        return std::nullopt;
    }

    TLV tlvPayload = map[Tag::PAYLOAD];

    return UPayload(tlvPayload.value.data(), tlvPayload.length, UPayloadType::VALUE);
}

bool MessageParser::isValidTag(Tag value) {
    return value >= Tag::UURI && value <= Tag::PAYLOAD;
}