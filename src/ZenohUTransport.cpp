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

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

#include <spdlog/spdlog.h>
#include <up-cpp/datamodel/serializer/UUri.h>
#include <up-cpp/datamodel/serializer/Uuid.h>

#include <stdexcept>

namespace uprotocol::transport {

constexpr char UATTRIBUTE_VERSION = 1;

constexpr uint32_t WILDCARD_ENTITY_ID = 0x0000FFFF;
constexpr uint32_t WILDCARD_ENTITY_VERSION = 0x000000FF;
constexpr uint32_t WILDCARD_RESOURCE_ID = 0x0000FFFF;

v1::UStatus ZenohUTransport::uError(v1::UCode code, std::string_view message) {
	v1::UStatus status;
	status.set_code(code);
	status.set_message(std::string(message));
	return status;
}

std::string ZenohUTransport::toZenohKeyString(
    const std::string& default_authority_name, const v1::UUri& source,
    const std::optional<v1::UUri>& sink) {
	std::ostringstream zenoh_key;

	auto writeUUri = [&](const v1::UUri& uuri) {
		zenoh_key << "/";

		// authority_name
		if (uuri.authority_name().empty()) {
			zenoh_key << default_authority_name;
		} else {
			zenoh_key << uuri.authority_name();
		}
		zenoh_key << "/";

		// ue_id
		if (uuri.ue_id() == WILDCARD_ENTITY_ID) {
			zenoh_key << "*";
		} else {
			zenoh_key << std::uppercase << std::hex << uuri.ue_id();
		}
		zenoh_key << "/";

		// ue_version_major
		if (uuri.ue_version_major() == WILDCARD_ENTITY_VERSION) {
			zenoh_key << "*";
		} else {
			zenoh_key << std::uppercase << std::hex << uuri.ue_version_major();
		}
		zenoh_key << "/";

		// resource_id
		if (uuri.resource_id() == WILDCARD_RESOURCE_ID) {
			zenoh_key << "*";
		} else {
			zenoh_key << std::uppercase << std::hex << uuri.resource_id();
		}
	};

	zenoh_key << "up";

	writeUUri(source);

	if (sink.has_value()) {
		writeUUri(*sink);
	} else {
		zenoh_key << "/{}/{}/{}/{}";
	}
	return zenoh_key.str();
}

std::vector<std::pair<std::string, std::vector<uint8_t>>>
ZenohUTransport::uattributesToAttachment(const v1::UAttributes& attributes) {
	std::vector<std::pair<std::string, std::vector<uint8_t>>> res;

	std::vector<uint8_t> version = {UATTRIBUTE_VERSION};

	std::vector<uint8_t> data(attributes.ByteSizeLong());
	attributes.SerializeToArray(data.data(), static_cast<int>(data.size()));

	res.emplace_back("", version);
	res.emplace_back("", data);
	return res;
}

v1::UAttributes ZenohUTransport::attachmentToUAttributes(
    const zenoh::Bytes& attachment) {
	auto attachment_vec = zenoh::ext::deserialize<
	    std::vector<std::pair<std::string, std::vector<uint8_t>>>>(attachment);

	if (attachment_vec.size() != 2) {
		spdlog::error("attachmentToUAttributes: attachment size != 2");
		// TODO: error report, exception?
	}

	if (attachment_vec[0].second.size() == 1) {
		if (attachment_vec[0].second[0] != UATTRIBUTE_VERSION) {
			spdlog::error("attachmentToUAttributes: incorrect version");
			// TODO: error report, exception?
		}
	};
	v1::UAttributes res;
	const std::vector<uint8_t> data = attachment_vec[1].second;
	res.ParseFromArray(data.data(), static_cast<int>(data.size()));
	return res;
}

zenoh::Priority ZenohUTransport::mapZenohPriority(v1::UPriority upriority) {
	switch (upriority) {
		case v1::UPriority::UPRIORITY_CS0:
			return Z_PRIORITY_BACKGROUND;
		case v1::UPriority::UPRIORITY_CS1:
			return Z_PRIORITY_DATA_LOW;
		case v1::UPriority::UPRIORITY_CS2:
			return Z_PRIORITY_DATA;
		case v1::UPriority::UPRIORITY_CS3:
			return Z_PRIORITY_DATA_HIGH;
		case v1::UPriority::UPRIORITY_CS4:
			return Z_PRIORITY_INTERACTIVE_LOW;
		case v1::UPriority::UPRIORITY_CS5:
			return Z_PRIORITY_INTERACTIVE_HIGH;
		case v1::UPriority::UPRIORITY_CS6:
			return Z_PRIORITY_REAL_TIME;
		// These sentinel values come from the protobuf compiler.
		// They are illegal for the enum, but cause linting problems.
		// In order to suppress the linting error, they need to
		// be included in the switch-case statement.
		// It is deemed acceptable to use an exception here because
		// it is in the sending code. An exception would not be
		// acceptable in receiving code. The correct strategy wopuld be
		// to drop the message.
		case v1::UPriority::UPriority_INT_MIN_SENTINEL_DO_NOT_USE_:
		case v1::UPriority::UPriority_INT_MAX_SENTINEL_DO_NOT_USE_:
			throw std::runtime_error(
			    "Sentinel values detected in priority switch-case");
		case v1::UPriority::UPRIORITY_UNSPECIFIED:
		default:
			return Z_PRIORITY_DATA_LOW;
	}
}

std::optional<v1::UMessage> ZenohUTransport::sampleToUMessage(
    const zenoh::Sample& sample) {
	v1::UMessage message;
	const auto attachment = sample.get_attachment();
	if (attachment.has_value()) {
		*message.mutable_attributes() =
		    attachmentToUAttributes(attachment.value());
	} else {
		spdlog::error(
		    "sampleToUMessage: empty attachment, cannot read uAttributes");
		return std::nullopt;
	}
	std::string payload(
	    zenoh::ext::deserialize<std::string>(sample.get_payload()));
	message.set_payload(payload);

	return message;
}

std::optional<v1::UMessage> ZenohUTransport::queryToUMessage(
    const zenoh::Query& query) {
	v1::UMessage message;
	const auto attachment = query.get_attachment();
	if (attachment.has_value()) {
		*message.mutable_attributes() =
		    attachmentToUAttributes(attachment.value());
	} else {
		spdlog::error(
		    "queryToUMessage: empty attachment, cannot read uAttributes");
		return std::nullopt;
	}
	if (query.get_payload().has_value()) {
		std::string payload(zenoh::ext::deserialize<std::string>(
		    query.get_payload().value().get()));
		message.set_payload(payload);
	}

	return message;
}

ZenohUTransport::ZenohUTransport(const v1::UUri& defaultUri,
                                 const std::filesystem::path& configFile)
    : UTransport(defaultUri),
      session_(zenoh::Session::open(
          std::move(zenoh::Config::from_file(configFile.string().c_str())))) {
	// TODO: add to setup or remove
	spdlog::set_level(spdlog::level::debug);

	spdlog::info("ZenohUTransport init");
}

v1::UStatus ZenohUTransport::registerPublishNotificationListener_(
    const std::string& zenoh_key, CallableConn listener) {
	spdlog::info("registerPublishNotificationListener_: {}", zenoh_key);

	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto on_sample = [this, listener](const zenoh::Sample& sample) mutable {
		auto maybeMessage = sampleToUMessage(sample);
		if (maybeMessage.has_value()) {
			listener(maybeMessage.value());
		} else {
			spdlog::error("on_sample: failed to retrieve uMessage");
		}
	};

	auto on_drop = []() {};

	auto subscriber = session_.declare_subscriber(
	    zenoh_key, std::move(on_sample), std::move(on_drop));
	subscriber_map_.emplace(listener, std::move(subscriber));
	return v1::UStatus();
}

v1::UStatus ZenohUTransport::sendPublishNotification_(
    const std::string& zenoh_key, const std::string& payload,
    const v1::UAttributes& attributes) {
	spdlog::debug("sendPublishNotification_: {}: {}", zenoh_key, payload);
	auto attachment = uattributesToAttachment(attributes);

	auto priority = mapZenohPriority(attributes.priority());

	try {
		// -Wpedantic disallows named member initialization until C++20,
		// so PutOptions needs to be explicitly created and passed with
		// std::move()
		zenoh::Session::PutOptions options;
		options.priority = priority;
		options.encoding = zenoh::Encoding("app/custom");
		options.attachment = zenoh::ext::serialize(attachment);
		session_.put(zenoh::KeyExpr(zenoh_key), zenoh::ext::serialize(payload),
		             std::move(options));
	} catch (const zenoh::ZException& e) {
		return uError(v1::UCode::INTERNAL, e.what());
	}

	return v1::UStatus();
}

// NOTE: Messages have already been validated by the base class. It does not
// need to be re-checked here.
v1::UStatus ZenohUTransport::sendImpl(const v1::UMessage& message) {
	const auto& payload = message.payload();

	const auto& attributes = message.attributes();

	std::string zenoh_key;
	if (attributes.type() == v1::UMessageType::UMESSAGE_TYPE_PUBLISH) {
		zenoh_key = toZenohKeyString(getEntityUri().authority_name(),
		                             attributes.source(), {});
	} else {
		zenoh_key = toZenohKeyString(getEntityUri().authority_name(),
		                             attributes.source(), attributes.sink());
	}

	return sendPublishNotification_(zenoh_key, payload, attributes);
}

v1::UStatus ZenohUTransport::registerListenerImpl(
    CallableConn&& listener, const v1::UUri& source_filter,
    std::optional<v1::UUri>&& sink_filter) {
	std::string zenoh_key = toZenohKeyString(getEntityUri().authority_name(),
	                                         source_filter, sink_filter);

	return registerPublishNotificationListener_(zenoh_key, listener);
}

void ZenohUTransport::cleanupListener(CallableConn listener) {
	subscriber_map_.erase(listener);
}

}  // namespace uprotocol::transport
