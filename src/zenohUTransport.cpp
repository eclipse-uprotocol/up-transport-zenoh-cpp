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

#include <uprotocol-cpp-ulink-zenoh/message/messageBuilder.h>
#include <uprotocol-cpp-ulink-zenoh/message/messageParser.h>
#include <uprotocol-cpp-ulink-zenoh/transport/zenohUTransport.h>
#include <uprotocol-cpp-ulink-zenoh/session/zenohSessionManager.h>
#include <uprotocol-cpp/uuid/serializer/UuidSerializer.h>
#include <uprotocol-cpp/uri/serializer/LongUriSerializer.h>
#include <spdlog/spdlog.h>
#include <zenoh.h>

using namespace std;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;

ZenohUTransport& ZenohUTransport::instance(void) noexcept {

    static ZenohUTransport zenohUtransport;

    return zenohUtransport;
}

UStatus ZenohUTransport::init() noexcept {
    
    UStatus status;

    if (0 == refCount_) {

        std::lock_guard<std::mutex> lock(mutex_);

        if (0 == refCount_) {

            /* by default initialized to empty strings */
            ZenohSessionManagerConfig sessionConfig;

            if (UCode::OK != ZenohSessionManager::instance().init(sessionConfig)) {
                spdlog::error("zenohSessionManager::instance().init() failed");
                status.set_code(UCode::INTERNAL);
                return status;
            }

            if (ZenohSessionManager::instance().getSession().has_value()) {
                session_ = ZenohSessionManager::instance().getSession().value();
            } else {
                status.set_code(UCode::INTERNAL);
                return status;
            }
        }

        refCount_.fetch_add(1);

    } else {
        refCount_.fetch_add(1);
    }
    
    spdlog::info("ZenohUTransport init done refCount = {}", refCount_);

    status.set_code(UCode::OK);

    return status;
}

UStatus ZenohUTransport::term() noexcept {

    UStatus status;

    std::lock_guard<std::mutex> lock(mutex_);

    refCount_.fetch_sub(1);

    if (0 == refCount_) {

        uint8_t retries;

        termPending_ = true;

        while ((0 != pendingSendRefCnt_)
            && (termMaxRetries_ > retries)) {

            std::this_thread::sleep_for(termRetryTimeout_);
            ++retries;
        }

        if (termMaxRetries_ == retries) {
            spdlog::error("timeout elapsed while trying to terminate");
            status.set_code(UCode::INTERNAL);
            return status;
        }

        for (auto pub : pubHandleMap_) {
            if (0 != z_undeclare_publisher(z_move(pub.second))) {
                //TODO - print the URI that failed 
                spdlog::error("z_undeclare_publisher failed");
                status.set_code(UCode::INVALID_ARGUMENT);
                return status;
            }
        }

        pubHandleMap_.clear();

        for (auto listenerInfo : listenerMap_) {
            for (auto sub : listenerInfo.second->subVector_) {
                if (0 != z_undeclare_subscriber(z_move(sub))) {
                    spdlog::error("z_undeclare_publisher failed");
                    status.set_code(UCode::INVALID_ARGUMENT);
                    return status;
                } else {
                    spdlog::debug("z_undeclare_subscriber done");
                }
             }
             
            listenerInfo.second->listenerVector_.clear();
            listenerInfo.second->subVector_.clear();
        }

    
        listenerMap_.clear();

        if (UCode::OK != ZenohSessionManager::instance().term()) {
            spdlog::error("zenohSessionManager::instance().term() failed");
            status.set_code(UCode::INTERNAL);
            return status;
        }

        spdlog::info("ZenohUTransport term done");
    }

    status.set_code(UCode::OK);

    return status;
}

UStatus ZenohUTransport::send(const UUri &uri, 
                              const UPayload &payload,
                              const UAttributes &attributes) noexcept {

    UStatus status;

    if (0 == refCount_) {
        spdlog::error("ZenohUTransport is not initialized");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }

    if (true == termPending_) {
        spdlog::error("ZenohUTransport is marked for termination");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }
    
    /* determine according to the URI is the send is and RPC response or a regular publish */
    if (false == uri.getUResource().isRPCMethod()) {
        status.set_code(sendPublish(uri, payload, attributes));
    } else {
        status.set_code(sendQueryable(uri, payload, attributes));
    }

    return status;
}

UCode ZenohUTransport::sendPublish(const UUri &uri, 
                                   const UPayload &payload,
                                   const UAttributes &attributes) noexcept {

    UCode status;

    status = UCode::UNAVAILABLE;
   
    do {

        if (UMessageType::PUBLISH != attributes.type()) {
            
            spdlog::error("Wrong message type = {}", UMessageTypeToString(attributes.type()).value());
            return UCode::INVALID_ARGUMENT;
        }

        /* get hash and check if the publisher for the URI is already exists */
        auto uriHash = std::hash<std::string>{}(LongUriSerializer::serialize(uri)); 
        auto handleInfo = pubHandleMap_.find(uriHash);    
    
        /* increment the number of pending send operations*/
        pendingSendRefCnt_.fetch_add(1);

        z_owned_publisher_t pub;

        /* check if the publisher exists */
        if (handleInfo == pubHandleMap_.end()) {
                     
            std::lock_guard<std::mutex> lock(pubInitMutex_);

            if (handleInfo != pubHandleMap_.end()) {
                pub = handleInfo->second;
            } else {
                
                pub = z_declare_publisher(z_loan(session_), z_keyexpr(std::to_string(uriHash).c_str()), nullptr);
              
                if (false == z_check(pub)) {
                    spdlog::error("Unable to declare Publisher for key expression!");
                    break;
                }

                pubHandleMap_[uriHash] = pub;
            }
        } else {
            pub = handleInfo->second;
        }

        z_publisher_put_options_t options = z_publisher_put_options_default();

        if (true == attributes.serializationHint().has_value()) {
            if (UCode::OK != mapEncoding(attributes.serializationHint().value(), options.encoding)) {
                spdlog::error("mapEncoding failure");
                break;
            }
        } else {
            options.encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
        }

        auto message = MessageBuilder::build(attributes, payload);
        if (0 == message.size()) {
            spdlog::error("MessageBuilder::build failed");
            break;
        }
        
        if (0 != z_publisher_put(z_loan(pub), message.data(), message.size(), &options)) {
            spdlog::error("z_publisher_put failed");
            break;
        }
        
       status = UCode::OK;

    } while(0);
        
    pendingSendRefCnt_.fetch_sub(1);

    return status;
}

UCode ZenohUTransport::sendQueryable(const UUri &uri, 
                                     const UPayload &payload,
                                     const UAttributes &attributes) noexcept {

    if (UMessageType::RESPONSE != attributes.type()) {
        spdlog::error("a Wrong message type = {}", UMessageTypeToString(attributes.type()).value());
        return UCode::INVALID_ARGUMENT;
    }

    auto uuidStr = UuidSerializer::serializeToString(attributes.id());

    if (queryMap_.find(uuidStr) == queryMap_.end()) {
        spdlog::error("failed to find uid = {}", uuidStr);
        return UCode::UNAVAILABLE;
    }

    auto query = queryMap_[uuidStr];

    z_query_reply_options_t options = z_query_reply_options_default();

    if (true == attributes.serializationHint().has_value()) {

        if (UCode::OK != mapEncoding(attributes.serializationHint().value(), options.encoding)) {
            spdlog::error("mapEncoding failure");
            return UCode::INTERNAL;
        }
    } else {
        options.encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
    }

    auto message = MessageBuilder::build(attributes, payload);
    if (0 == message.size()) {
        spdlog::error("MessageBuilder::instance().build failed");
        return UCode::INTERNAL;
    }

    z_query_t lquery = z_loan(query);

    if (0 != z_query_reply(&lquery, z_query_keyexpr(&lquery), message.data(), message.size(), &options)) {
        spdlog::error("z_query_reply failed");
        return UCode::INTERNAL;
    }
 
    auto keyStr = z_keyexpr_to_string(z_query_keyexpr(&lquery));

    z_drop(z_move(keyStr));
    z_drop(z_move(query));
    /* once replied remove the uuid from the map , as it cannot be reused */
    queryMap_.erase(uuidStr);

    spdlog::debug("replied on query with uid = {}", std::string(uuidStr));

    return UCode::OK;
}

UStatus ZenohUTransport::registerListener(const UUri &uri,
                                          const UListener &listener) noexcept {
   
    UStatus status;

    cbArgumentType* arg;
    std::shared_ptr<ListenerContainer> listenerContainer;

    if (0 == refCount_) {
        spdlog::error("ZenohUTransport is not initialized");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }

    if (true == termPending_) {
        spdlog::error("ZenohUTransport is marked for termination");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }

    do {

        status.set_code(UCode::OK);
        arg = nullptr;
        listenerContainer = nullptr;

        std::lock_guard<std::mutex> lock(subInitMutex_);

        auto uriHash = std::hash<std::string>{}(LongUriSerializer::serialize(uri));

        // check if URI exists 
        if (listenerMap_.find(uriHash) != listenerMap_.end()) {

            listenerContainer = listenerMap_[uriHash];

            for (const UListener *existingListenerPtr : listenerContainer->listenerVector_) {
                if (existingListenerPtr == &listener) {
                    spdlog::error("listener already set for URI");
                    status.set_code(UCode::INVALID_ARGUMENT);
                    break;
                }
            }
            
            if (UCode::OK != status.code()) {
               break;
            }
        }

        if (nullptr == listenerContainer) {
            listenerContainer = make_shared<ListenerContainer>();
            if (nullptr == listenerContainer) {
                spdlog::error("listenerContainer allocation failure");
                status.set_code(UCode::INTERNAL);
                break;
            }
        }
        
        arg = new cbArgumentType(uri, this, listener);
        if (nullptr == arg) {
            spdlog::error("failed to allocate arguments for callback");
            status.set_code(UCode::INTERNAL);
            break;
        }

        /* listener for a regular pub-sub*/
        if (false == uri.getUResource().isRPCMethod()) {

            z_owned_closure_sample_t callback = z_closure(SubHandler, OnSubscriberClose, arg);

            auto sub = z_declare_subscriber(z_loan(session_), z_keyexpr(std::to_string(uriHash).c_str()), z_move(callback), nullptr);
            if (!z_check(sub)) {
                spdlog::error("z_declare_subscriber failed");
                status.set_code(UCode::INTERNAL);
                break;
            }
            
            listenerContainer->subVector_.push_back(sub);
            listenerContainer->listenerVector_.push_back(&listener);
        }

         /* listener for a RPC*/
        if (true == uri.getUResource().isRPCMethod()) {

            z_owned_closure_query_t callback = z_closure(QueryHandler, OnQueryClose, arg);
        
            auto qable = z_declare_queryable(z_loan(session_), z_keyexpr(std::to_string(uriHash).c_str()), z_move(callback), nullptr);
            if (!z_check(qable)) {
                spdlog::error("failed to create queryable");
                status.set_code(UCode::INTERNAL);
                break;
            }

            listenerContainer->queryVector_.push_back(qable);
            listenerContainer->listenerVector_.push_back(&listener);
        }

        listenerMap_[uriHash] = listenerContainer;

        status.set_code(UCode::OK);

    } while(0);

    if (UCode::OK != status.code()) {
        
        if (nullptr != listenerContainer) {
            listenerContainer.reset();
        }

        if (nullptr != arg) {
            delete arg;
        }
    }

    return status;
}

UStatus ZenohUTransport::unregisterListener(const UUri &uri, 
                                            const UListener &listener) noexcept {

    UStatus status;

    std::shared_ptr<ListenerContainer> listenerContainer;

    if (0 == refCount_) {
        spdlog::error("ZenohUTransport is not initialized");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }

    if (true == termPending_) {
        spdlog::error("ZenohUTransport is marked for termination");
        status.set_code(UCode::UNAVAILABLE);
        return status;
    }

    auto uriHash = std::hash<std::string>{}(LongUriSerializer::serialize(uri));

    if (listenerMap_.find(uriHash) == listenerMap_.end()) {
        status.set_code(UCode::INVALID_ARGUMENT);
        return status;
    }

    listenerContainer = listenerMap_[uriHash];

    int32_t index = 0;

    /* need to check with who the listener is associated */
    for (const UListener *existingListenerPtr : listenerContainer->listenerVector_) {

        if (&listener == existingListenerPtr) {

            listenerContainer->listenerVector_.erase(listenerContainer->listenerVector_.begin() + index);

            if (false == listenerContainer->subVector_.empty()){
                z_undeclare_subscriber(z_move(listenerContainer->subVector_[index]));
                listenerContainer->subVector_.erase(listenerContainer->subVector_.begin() + index);
            } else {
                z_undeclare_queryable(z_move(listenerContainer->queryVector_[index]));
                listenerContainer->queryVector_.erase(listenerContainer->queryVector_.begin() + index);
            }
            break;
        }

        ++index;
    }

    status.set_code(UCode::OK);
    return status;
}

void ZenohUTransport::SubHandler(const z_sample_t* sample,
                                 void* arg) {

    cbArgumentType *tuplePtr = static_cast<cbArgumentType*>(arg);

    auto uri = get<0>(*tuplePtr);
    auto listener = &get<2>(*tuplePtr);

    auto tlvVector = MessageParser::getAllTlv(sample->payload.start, sample->payload.len);
    
    if (false == tlvVector.has_value()) {
        spdlog::error("MessageParser::getAllTlv failure");
        return;
    }

    auto attributes = MessageParser::getAttributes(tlvVector.value());
    if (false == attributes.has_value()) {
        spdlog::error("getAttributes failure");
        return;
    }

    auto payload = MessageParser::getPayload(tlvVector.value());    
    if (false == payload.has_value()) {
        spdlog::error("getPayload failure");
        return;
    }

    if (UCode::OK != listener->onReceive(uri, payload.value(), attributes.value()).code()) {
        /*TODO error handling*/
        spdlog::error("listener->onReceive failed");
        return;
    }
}

void ZenohUTransport::QueryHandler(const z_query_t *query, 
                                   void *arg) 
{
    cbArgumentType *tuplePtr = static_cast<cbArgumentType*>(arg);

    auto uri = get<0>(*tuplePtr);
    auto instance = get<1>(*tuplePtr);
    auto listener = &get<2>(*tuplePtr);

    z_owned_query_t oquery = z_query_clone(query);
    z_value_t payload_value = z_query_value(query);

    auto tlvVector = MessageParser::getAllTlv(payload_value.payload.start, payload_value.payload.len);
     
    if (false == tlvVector.has_value()) {
        spdlog::error("getAllTlv failure");
        return;
    }

    auto attributes = MessageParser::getAttributes(tlvVector.value());
    if (false == attributes.has_value()) {
        spdlog::error("getAttributes failure");
        return;
    }

    auto payload = MessageParser::getPayload(tlvVector.value());    
    if (false == payload.has_value()) {
        spdlog::error("getPayload failure");
        return;
    }

    auto uuidStr = UuidSerializer::serializeToString(attributes->id());

    instance->queryMap_[uuidStr] = oquery;

    if (UMessageType::REQUEST != attributes.value().type()) {
        spdlog::error("Wrong message type = {}", UMessageTypeToString(attributes.value().type()).value());
        return;
    }
  
    if (UCode::OK != listener->onReceive(uri, payload.value(), attributes.value()).code()) {
       /*TODO error handling*/
       spdlog::error("onReceive failure");
       return;
    }                                 
}

UCode ZenohUTransport::mapEncoding(const USerializationHint &encodingIn, 
                                   z_encoding_t &encodingOut) noexcept {

    switch (encodingIn) {
        case USerializationHint::PROTOBUF: {
            encodingOut = z_encoding(Z_ENCODING_PREFIX_APP_OCTET_STREAM, nullptr);
        }
        break;
        case USerializationHint::JSON: {
            encodingOut = z_encoding(Z_ENCODING_PREFIX_APP_JSON, nullptr);
        }
        break;
        case USerializationHint::SOMEIP: {
            encodingOut = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
        }
        break;
        case USerializationHint::RAW: {
            encodingOut = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
        }
        break;
        case USerializationHint::UNKNOWN: 
        default: {
            return UCode::UNAVAILABLE; 
        }
    }

    return UCode::OK;
}

UStatus ZenohUTransport::receive(const UUri &uri, 
                                 const UPayload &payload, 
                                 const UAttributes &attributes) noexcept {

    UStatus status;

    (void)uri;
    (void)payload;
    (void)attributes;

    spdlog::error("not implemented");

    status.set_code(UCode::UNAVAILABLE);

    return status;
}

void ZenohUTransport::OnSubscriberClose(void *arg) {

    if (nullptr == arg) {
        spdlog::error("arg is nullptr");
    } else {

        cbArgumentType *info = 
            reinterpret_cast<cbArgumentType*>(arg);

        delete info;
    }
}

void ZenohUTransport::OnQueryClose(void *arg) {

    if (nullptr == arg) {
        spdlog::error("arg is nullptr");
    } else {

        cbArgumentType *info = 
            reinterpret_cast<cbArgumentType*>(arg);

        delete info;
    }
}
