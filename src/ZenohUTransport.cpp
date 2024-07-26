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

#include <up-cpp/datamodel/serializer/UUri.h>
#include <up-cpp/datamodel/serializer/Uuid.h>
#include <up-cpp/datamodel/validator/UMessage.h>
#include <up-cpp/datamodel/validator/UUri.h>

#include <stdexcept>

namespace uprotocol::transport {

const char UATTRIBUTE_VERSION = 1;

const uint32_t WILDCARD_ENTITY_ID = 0x0000FFFF;
const uint32_t WILDCARD_ENTITY_VERSION = 0x000000FF;
const uint32_t WILDCARD_RESOURCE_ID = 0x0000FFFF;

using namespace zenoh;
using namespace uprotocol::v1;
using namespace uprotocol::datamodel;

UStatus ZenohUTransport::uError(UCode code, std::string_view message) {
	UStatus status;
	status.set_code(code);
	status.set_message(std::string(message));
	return status;
}

std::string ZenohUTransport::toZenohKeyString(
    const std::string& default_authority_name, const UUri& source,
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
			zenoh_key << uuri.ue_id();
		}
		zenoh_key << "/";

		// ue_version_major
		if (uuri.ue_version_major() == WILDCARD_ENTITY_VERSION) {
			zenoh_key << "*";
		} else {
			zenoh_key << uuri.ue_version_major();
		}
		zenoh_key << "/";

		// resource_id
		if (uuri.resource_id() == WILDCARD_RESOURCE_ID) {
			zenoh_key << "*";
		} else {
			zenoh_key << uuri.resource_id();
		}
	};

	zenoh_key << "up";
	zenoh_key << std::uppercase << std::hex;

	writeUUri(source);

	if (sink.has_value()) {
		writeUUri(*sink);
	} else {
		zenoh_key << "/{}/{}/{}/{}";
	}
	return zenoh_key.str();
}

std::vector<std::pair<std::string, std::string>>
ZenohUTransport::uattributesToAttachment(const UAttributes& attributes) {
	std::vector<std::pair<std::string, std::string>> res;

	std::string version(&UATTRIBUTE_VERSION, 1);

	std::string data;
	attributes.SerializeToString(&data);

	res.push_back(std::make_pair("", version));
	res.push_back(std::make_pair("", data));
	return res;
}

UAttributes ZenohUTransport::attachmentToUAttributes(
    const AttachmentView& attachment) {
	std::vector<BytesView> attachment_vec;
	attachment.iterate(
	    [&](const BytesView& key, const BytesView& value) -> bool {
		    attachment_vec.push_back(value);
		    return true;
	    });

	if (attachment_vec.size() != 2) {
		// TODO: error report, exception?
	}

	if (attachment_vec[0].get_len() == 1) {
		if (attachment_vec[0].as_string_view()[0] != UATTRIBUTE_VERSION) {
			// TODO: error report, exception?
		}
	};
	UAttributes res;
	// TODO: more efficient way?
	res.ParseFromString(std::string(attachment_vec[1].as_string_view()));
	return res;
}

Priority ZenohUTransport::mapZenohPriority(UPriority upriority) {
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

UMessage ZenohUTransport::sampleToUMessage(const Sample& sample) {
	UMessage message;
	if (sample.get_attachment().check()) {
		*message.mutable_attributes() =
		    attachmentToUAttributes(sample.get_attachment());
	}
	std::string payload(sample.get_payload().as_string_view());
	message.set_payload(payload);

	return message;
}

ZenohUTransport::ZenohUTransport(const UUri& defaultUri,
                                 const std::filesystem::path& configFile)
    : UTransport(defaultUri),
      session_(expect<Session>(open(
          std::move(expect(config_from_file(configFile.string().c_str())))))) {}

UStatus ZenohUTransport::registerRequestListener_(const std::string& zenoh_key,
                                                  CallableConn listener) {
	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto on_query = [this, listener](const Query& query) {
		UAttributes attributes;
		if (query.get_attachment().check()) {
			attributes = attachmentToUAttributes(query.get_attachment());
		}
		auto id_str = serializer::uuid::AsString().serialize(attributes.id());
		std::unique_lock<std::mutex> lock(query_map_mutex_);
		query_map_.insert(std::make_pair(
		    std::move(id_str), std::move(std::make_shared<OwnedQuery>(query))));
	};

	auto on_drop_queryable = []() {};

	auto queryable = expect<Queryable>(
	    session_.declare_queryable(zenoh_key, {on_query, on_drop_queryable}));

	return UStatus();
}

UStatus ZenohUTransport::registerResponseListener_(const std::string& zenoh_key,
                                                   CallableConn listener) {
	std::unique_lock<std::mutex> lock(rpc_callback_map_mutex_);
	rpc_callback_map_.insert(std::make_pair(zenoh_key, listener));

	return UStatus();
}

UStatus ZenohUTransport::registerPublishNotificationListener_(
    const std::string& zenoh_key, CallableConn listener) {
	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto data_handler = [this, listener](const Sample& sample) mutable {
		listener(sampleToUMessage(sample));
		// invoke_nonblock_callback(&cb_sender, &listener_cloned, Ok(msg));
	};

	auto key = ListenerKey(listener, zenoh_key);
	auto subscriber = expect<Subscriber>(
	    session_.declare_subscriber(zenoh_key, data_handler));
	{
		std::unique_lock<std::mutex> lock(subscriber_map_mutex_);
		subscriber_map_.insert(
		    std::make_pair(std::move(key), std::move(subscriber)));
	}
	return UStatus();
}

UStatus ZenohUTransport::sendRequest_(const std::string& zenoh_key,
                                      const std::string& payload,
                                      const UAttributes& attributes) {
	auto source_str =
	    serializer::uri::AsString().serialize(attributes.source());
	CallableConn resp_callback;
	{
		std::unique_lock<std::mutex> lock(rpc_callback_map_mutex_);

		if (auto resp_callback_it = rpc_callback_map_.find(source_str);
		    resp_callback_it == rpc_callback_map_.end()) {
			return uError(UCode::UNAVAILABLE, "failed to find UUID");
		} else {
			resp_callback = resp_callback_it->second;
		}
	}
	auto on_reply = [&](Reply&& reply) {
		auto result = reply.get();

		if (auto sample = std::get_if<Sample>(&result)) {
			resp_callback(sampleToUMessage(*sample));
		} else if (auto error = std::get_if<ErrorMessage>(&result)) {
			// TODO: error report
		}
	};

	auto attachment = uattributesToAttachment(attributes);

	GetOptions options;
	options.set_target(Z_QUERY_TARGET_BEST_MATCHING);
	options.set_value(Value(payload));
	options.set_attachment(attachment);

	auto onDone = []() {};

	session_.get(zenoh_key, "", {on_reply, onDone}, options);

	return UStatus();
}

UStatus ZenohUTransport::sendResponse_(const std::string& payload,
                                       const UAttributes& attributes) {
	auto reqid_str = serializer::uuid::AsString().serialize(attributes.reqid());
	OwnedQueryPtr query;
	{
		std::unique_lock<std::mutex> lock(query_map_mutex_);
		if (auto query_it = query_map_.find(reqid_str);
		    query_it == query_map_.end()) {
			return uError(UCode::INTERNAL, "query doesn't exist");
		} else {
			query = query_it->second;
		}
	}

	QueryReplyOptions options;
	query->loan().reply(query->loan().get_keyexpr().as_string_view(), payload,
	                    options);

	return UStatus();
}

UStatus ZenohUTransport::sendPublishNotification_(
    const std::string& zenoh_key, const std::string& payload,
    const UAttributes& attributes) {
	auto attachment = uattributesToAttachment(attributes);

	auto priority = mapZenohPriority(attributes.priority());

	PutOptions options;
	options.set_encoding(Z_ENCODING_PREFIX_APP_CUSTOM);
	options.set_priority(priority);
	options.set_attachment(attachment);
	if (!session_.put(zenoh_key, payload, options)) {
		return uError(UCode::INTERNAL, "Unable to send with Zenoh");
	}

	return UStatus();
}

// NOTE: Messages have already been validated by the base class. It does not
// need to be re-checked here.
v1::UStatus ZenohUTransport::sendImpl(const UMessage& message) {
	const auto& payload = message.payload();

	const auto& attributes = message.attributes();

	std::string zenoh_key;
	if (attributes.type() == UMessageType::UMESSAGE_TYPE_PUBLISH) {
		zenoh_key = toZenohKeyString(getDefaultSource().authority_name(),
		                             attributes.source(), {});
	} else {
		zenoh_key = toZenohKeyString(getDefaultSource().authority_name(),
		                             attributes.source(), attributes.sink());
	}

	switch (attributes.type()) {
		case UMessageType::UMESSAGE_TYPE_PUBLISH: {
			return sendPublishNotification_(zenoh_key, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_NOTIFICATION: {
			return sendPublishNotification_(zenoh_key, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_REQUEST: {
			return sendRequest_(zenoh_key, payload, attributes);
		}
		case UMessageType::UMESSAGE_TYPE_RESPONSE: {
			return sendResponse_(payload, attributes);
		}
		default: {
			return uError(UCode::INVALID_ARGUMENT,
			              "Wrong Message type in UAttributes");
		}
	}
	return UStatus();
}

v1::UStatus ZenohUTransport::registerListenerImpl(
    CallableConn&& listener, const v1::UUri& source_filter,
    std::optional<v1::UUri>&& sink_filter) {
	std::string zenoh_key = toZenohKeyString(
	    getDefaultSource().authority_name(), source_filter, sink_filter);
	if (!sink_filter) {
		// When only a single filter is provided, this signals that the
		// listener is for a pub/sub-like communication mode where then
		// messages are expected to only have a source address.
		registerPublishNotificationListener_(zenoh_key, listener);
	} else {
		// Otherwise, the filters could be for any communication mode.
		// We can't use the UUri validators to determine what mode they
		// are for because a) there is overlap in allowed values between
		// modes and b) any filter is allowed to have wildcards present.
		registerResponseListener_(zenoh_key, listener);
		registerRequestListener_(zenoh_key, listener);
		registerPublishNotificationListener_(zenoh_key, listener);
	}

	v1::UStatus status;
	status.set_code(v1::UCode::OK);
	return status;
}

void ZenohUTransport::cleanupListener(CallableConn listener) {}

}  // namespace uprotocol::transport
