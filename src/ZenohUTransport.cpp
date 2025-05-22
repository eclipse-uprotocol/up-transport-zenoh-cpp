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
#include <up-cpp/datamodel/validator/UUri.h>

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

	auto write_u_uri = [&](const v1::UUri& uuri) {
		zenoh_key << "/";

		// authority_name
		if (uuri.authority_name().empty()) {
			zenoh_key << default_authority_name;
		} else {
			zenoh_key << uuri.authority_name();
		}
		zenoh_key << "/";

		// ue_id -> type id & instance id
		if (uprotocol::datamodel::validator::uri::has_wildcard_service_id(
		        uuri)) {
			zenoh_key << "*";
		} else {
			uint16_t service_id = uuri.ue_id() & 0xFFFF;
			zenoh_key << std::uppercase << std::hex << service_id;
		}
		zenoh_key << "/";

		if (uprotocol::datamodel::validator::uri::
		        has_wildcard_service_instance_id(uuri)) {
			zenoh_key << "*";
		} else {
			uint16_t service_instance_id = (uuri.ue_id() >> 16) & 0xFFFF;
			zenoh_key << std::uppercase << std::hex << service_instance_id;
		}
		zenoh_key << "/";

		// ue_version_major
		if (uprotocol::datamodel::validator::uri::has_wildcard_version(uuri)) {
			zenoh_key << "*";
		} else {
			zenoh_key << std::uppercase << std::hex << uuri.ue_version_major();
		}
		zenoh_key << "/";

		// resource_id
		if (uprotocol::datamodel::validator::uri::has_wildcard_resource_id(
		        uuri)) {
			zenoh_key << "*";
		} else {
			zenoh_key << std::uppercase << std::hex << uuri.resource_id();
		}
	};

	zenoh_key << "up";

	write_u_uri(source);

	if (sink.has_value()) {
		write_u_uri(*sink);
	} else {
		zenoh_key << "/{}/{}/{}/{}/{}";
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
		// TODO(unknown) error report, exception?
	}

	if (attachment_vec[0].second.size() == 1) {
		if (attachment_vec[0].second[0] != UATTRIBUTE_VERSION) {
			spdlog::error("attachmentToUAttributes: incorrect version");
			// TODO(unknown) error report, exception?
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
	auto payload(
	    zenoh::ext::deserialize<std::vector<uint8_t>>(sample.get_payload()));

	if (!payload.empty()) {
		std::string payload_as_string(payload.begin(), payload.end());
		message.set_payload(std::move(payload_as_string));
	}

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
		auto payload(zenoh::ext::deserialize<std::string>(
		    query.get_payload().value().get()));
		message.set_payload(payload);
	}

	return message;
}

ZenohUTransport::ZenohUTransport(const v1::UUri& default_uri,
                                 const std::filesystem::path& config_file)
    : UTransport(default_uri),
      session_(zenoh::Session::open(
          zenoh::Config::from_file(config_file.string()))) {
	// TODO(unknown) add to setup or remove
	spdlog::set_level(spdlog::level::debug);

	spdlog::info("ZenohUTransport init");
}

v1::UStatus ZenohUTransport::registerPublishNotificationListener_(
    const std::string& zenoh_key, CallableConn listener) {
	spdlog::info("registerPublishNotificationListener_: {}", zenoh_key);

	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto on_sample = [listener](const zenoh::Sample& sample) mutable {
		auto maybe_message = sampleToUMessage(sample);
		if (maybe_message.has_value()) {
			listener(maybe_message.value());
		} else {
			spdlog::error("on_sample: failed to retrieve uMessage");
		}
	};

	auto on_drop = []() {};

	auto subscriber = session_.declare_subscriber(
	    zenoh_key, std::move(on_sample), std::move(on_drop));
	subscriber_map_.emplace(listener, std::move(subscriber));
	return {};
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

		const std::vector<uint8_t> payload_as_bytes(payload.begin(),
		                                            payload.end());
		session_.put(zenoh::KeyExpr(zenoh_key),
		             zenoh::ext::serialize(payload_as_bytes),
		             std::move(options));
		spdlog::debug("sendPublishNotification_: sent successfully.");
	} catch (const zenoh::ZException& e) {
		spdlog::error(
		    "sendPublishNotification_: Error when sending message: {}",
		    e.what());
		return uError(v1::UCode::INTERNAL, e.what());
	}

	return {};
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

	return registerPublishNotificationListener_(zenoh_key, std::move(listener));
}

void ZenohUTransport::cleanupListener(const CallableConn& listener) {
	subscriber_map_.erase(listener);
}

}  // namespace uprotocol::transport
