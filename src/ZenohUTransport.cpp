// SPDX-FileCopyrightText: 2024 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0

#include <up-cpp/datamodel/serializer/UUri.h>
#include <up-cpp/datamodel/validator/UMessage.h>

#include <stdexcept>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace uprotocol::transport {

using namespace zenoh;
using namespace uprotocol::v1;
using namespace uprotocol::datamodel;

namespace {
/*
std::string to_hex_string(uint8_t byte) {
    std::stringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(byte);
    return ss.str();
}

std::string get_uauth_from_uuri(const UUri& uri) {
    if (!uri.authority_name().empty()) {
        std::vector<uint8_t> buf;
        try {
            buf = uri.to_bytes();
        } catch (const std::runtime_error& e) {
            std::string msg = "Unable to transform UAuthority into micro form";
            std::cerr << msg << std::endl;
            throw UStatus::fail_with_code(UCode::INVALID_ARGUMENT, msg);
        }

        std::ostringstream oss;
        for (auto c : buf) {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(c);
        }
        return oss.str();
    } else {
        std::string msg = "UAuthority is empty";
        std::cerr << msg << std::endl;
        throw UStatus::fail_with_code(UCode::INVALID_ARGUMENT, msg);
    }
}
*/
std::string toZenohKeyString(const UUri& uri) {
	/*
	if (uri.authority_name().has_value() && !uri.entity.has_value() &&
	    !uri.resource.has_value()) {
	    try {
	        return "upr/" + UPClientZenoh::get_uauth_from_uuri(uri) + "/**";
	    } catch (const std::runtime_error& e) {
	        std::string msg =
	            "Unable to get authority from UUri: " + std::string(e.what());
	        std::cerr << msg << std::endl;
	        throw UStatus::fail_with_code(UCode::INVALID_ARGUMENT, msg);
	    }
	} else {
	    std::vector<uint8_t> micro_uuri;
	    try {
	        micro_uuri = uri.to_bytes();
	    } catch (const std::runtime_error& e) {
	        std::string msg = "Unable to serialize into micro format: " +
	                          std::string(e.what());
	        std::cerr << msg << std::endl;
	        throw UStatus::fail_with_code(UCode::INVALID_ARGUMENT, msg);
	    }

	    std::string micro_zenohKey;
	    if (micro_uuri.size() > 8) {
	        micro_zenohKey = "upr/";
	        for (size_t i = 8; i < micro_uuri.size(); ++i) {
	            micro_zenohKey += to_hex_string(micro_uuri[i]);
	        }
	        micro_zenohKey += "/";
	    } else {
	        micro_zenohKey = "upl/";
	    }

	    for (size_t i = 0; i < 8 && i < micro_uuri.size(); ++i) {
	        micro_zenohKey += to_hex_string(micro_uuri[i]);
	    }

	    return micro_zenohKey;
	}
	*/
	return "";
}

std::map<std::string, std::string> uattributesToAttachment(
    const UAttributes& attributes) {
	std::map<std::string, std::string> res;
	return res;
}

UAttributes attachmentToUattributes(const AttachmentView& attachment) {
	UAttributes res;
	return res;
}

Priority mapZenohPriority(UPriority upriority) {
	switch (upriority) {
		case UPriority::UPRIORITY_CS0:
			return Z_PRIORITY_BACKGROUND;
		case UPriority::UPRIORITY_CS1:
			return Z_PRIORITY_DATA_LOW;
		case UPriority::UPRIORITY_CS2:
			return Z_PRIORITY_DATA;
		case UPriority::UPRIORITY_CS3:
			return Z_PRIORITY_DATA_HIGH;
		case UPriority::UPRIORITY_CS4:
			return Z_PRIORITY_INTERACTIVE_LOW;
		case UPriority::UPRIORITY_CS5:
			return Z_PRIORITY_INTERACTIVE_HIGH;
		case UPriority::UPRIORITY_CS6:
			return Z_PRIORITY_REAL_TIME;
		case UPriority::UPRIORITY_UNSPECIFIED:
		default:
			return Z_PRIORITY_DATA_LOW;
	}
}

}  // namespace

ZenohUTransport::ZenohUTransport(const UUri& defaultUri,
                                 const std::filesystem::path& configFile)
    : UTransport(defaultUri),
      session_(expect<Session>(open(
          std::move(expect(config_from_file(configFile.string().c_str())))))) {}

UStatus ZenohUTransport::sendPublishNotification_(
    const std::string& zenohKey, const std::string& payload,
    const UAttributes& attributes) {
	auto attachment = uattributesToAttachment(attributes);

	auto priority = mapZenohPriority(attributes.priority());

	PutOptions options;
	options.set_encoding(Z_ENCODING_PREFIX_APP_CUSTOM);
	options.set_priority(priority);
	options.set_attachment(attachment);
	if (!session_.put(zenohKey, payload, options)) {
		return UStatus();  // TODO: UCode::INTERNAL, "Unable to send with Zenoh"
	}

	return UStatus();
}

UStatus ZenohUTransport::sendRequest_(const std::string& zenohKey,
                                      const std::string& payload,
                                      const UAttributes& attributes) {
	auto uuidStr = serializer::uri::AsString().serialize(attributes.source());
	CallableConn respCallback;
	{
		std::unique_lock<std::mutex> lock(rpcCallbackMapMutex_);

		if (auto respCallbackIt = rpcCallbackMap_.find(uuidStr);
		    respCallbackIt == rpcCallbackMap_.end()) {
			return UStatus();  // TODO: UCode::UNAVAILABLE, "failed to find UUID
			                   // = {}", uuidStr

		} else {
			respCallback = respCallbackIt->second;
		}
	}
	auto onReply = [&](Reply&& reply) {
		auto result = reply.get();

		if (auto sample = std::get_if<Sample>(&result)) {
			UAttributes attributes;
			if (sample->get_attachment().check()) {
				attributes = attachmentToUattributes(sample->get_attachment());
			}
			std::string payload(sample->get_payload().as_string_view());
			UMessage message;
			message.set_payload(payload);
			message.set_allocated_attributes(&attributes);
			(*respCallback)(message);
		} else if (auto error = std::get_if<ErrorMessage>(&result)) {
			std::cout << "Received an error :" << error->as_string_view()
			          << "\n";
		}
	};

	auto attachment = uattributesToAttachment(attributes);

	GetOptions options;
	options.set_target(Z_QUERY_TARGET_BEST_MATCHING);
	options.set_value(Value(payload));
	options.set_attachment(attachment);

	auto onDone = []() {};

	session_.get(zenohKey, "", {onReply, onDone}, options);

	return UStatus();
}

UStatus ZenohUTransport::sendResponse_(const std::string& payload,
                                       const UAttributes& attributes) {
	return UStatus();
}

v1::UStatus ZenohUTransport::sendImpl(const UMessage& message) {
	if (!message.has_payload()) {
		std::string msg = "Invalid UPayload";
		return UStatus();  // TODO: UCode::INVALID_ARGUMENT, msg
	}
	const auto& payload = message.payload();

	const auto& attributes = message.attributes();
	if (attributes.type() == UMessageType::UMESSAGE_TYPE_UNSPECIFIED) {
		std::string msg = "Invalid UAttributes";
		return UStatus();  // TODO: UCode::INVALID_ARGUMENT, msg
	}

	switch (attributes.type()) {
		case UMessageType::UMESSAGE_TYPE_PUBLISH: {
			auto res = validator::message::isValidPublish(message);
			// TODO: check res
			std::string zenohKey = toZenohKeyString(attributes.source());
			return sendPublishNotification_(zenohKey, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_NOTIFICATION: {
			auto res = validator::message::isValidNotification(message);
			// TODO: check res
			std::string zenohKey = toZenohKeyString(attributes.sink());
			return sendPublishNotification_(zenohKey, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_REQUEST: {
			auto res = validator::message::isValidRpcRequest(message);
			// TODO: check res
			std::string zenohKey = toZenohKeyString(attributes.sink());
			return sendRequest_(zenohKey, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_RESPONSE: {
			auto res = validator::message::isValidRpcResponse(message);
			// TODO: check res
			return sendResponse_(payload, attributes);
		}
		default: {
			std::string msg = "Wrong Message type in UAttributes";
			return UStatus();  // TODO: UCode::INVALID_ARGUMENT, msg
		}
	}
	return UStatus();
}

v1::UStatus ZenohUTransport::registerListenerImpl(
    const v1::UUri& sink_filter, CallableConn&& listener,
    std::optional<v1::UUri>&& source_filter) {
	return v1::UStatus();
}

void ZenohUTransport::cleanupListener(CallableConn listener) {}

}  // namespace uprotocol::transport
