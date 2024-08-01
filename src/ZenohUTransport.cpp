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
ZenohUTransport::uattributesToAttachment(const v1::UAttributes& attributes) {
	std::vector<std::pair<std::string, std::string>> res;

	std::string version(&UATTRIBUTE_VERSION, 1);

	std::string data;
	attributes.SerializeToString(&data);

	res.emplace_back("", version);
	res.emplace_back("", data);
	return res;
}

v1::UAttributes ZenohUTransport::attachmentToUAttributes(
    const zenoh::Bytes& attachment) {
	auto attachment_vec =
	    attachment
	        .deserialize<std::vector<std::pair<std::string, std::string>>>();

	if (attachment_vec.size() != 2) {
		// TODO: error report, exception?
	}

	if (attachment_vec[0].second.size() == 1) {
		if (attachment_vec[0].second[0] != UATTRIBUTE_VERSION) {
			// TODO: error report, exception?
		}
	};
	v1::UAttributes res;
	res.ParseFromString(attachment_vec[1].second);
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
		case v1::UPriority::UPRIORITY_UNSPECIFIED:
		default:
			return Z_PRIORITY_DATA_LOW;
	}
}

v1::UMessage ZenohUTransport::sampleToUMessage(const zenoh::Sample& sample) {
	v1::UMessage message;
	*message.mutable_attributes() =
	    attachmentToUAttributes(sample.get_attachment());
	std::string payload(sample.get_payload().deserialize<std::string>());
	message.set_payload(payload);

	return message;
}

ZenohUTransport::ZenohUTransport(const v1::UUri& defaultUri,
                                 const std::filesystem::path& configFile)
    : UTransport(defaultUri),
      session_(zenoh::Session::open(
          std::move(zenoh::Config::from_file(configFile.string().c_str())))) {}

v1::UStatus ZenohUTransport::registerRequestListener_(
    const std::string& zenoh_key, CallableConn listener) {
	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto on_query = [this, listener](const zenoh::Query& query) {
		auto attributes = attachmentToUAttributes(query.get_attachment());
		auto id_str =
		    datamodel::serializer::uuid::AsString().serialize(attributes.id());
		std::unique_lock<std::mutex> lock(query_map_mutex_);

		// TODO(sashacmc): Replace this workaround with `query.clone()`
		// after zenohcpp 1.0.0-rc6 release
		zenoh::Query cloned_query(nullptr);
		z_query_clone(zenoh::detail::as_owned_c_ptr(cloned_query),
		              zenoh::detail::loan(query));

		query_map_.emplace(std::move(id_str), std::move(cloned_query));
	};

	auto on_drop = []() {};

	auto queryable = session_.declare_queryable(zenoh_key, std::move(on_query),
	                                            std::move(on_drop));

	return v1::UStatus();
}

v1::UStatus ZenohUTransport::registerResponseListener_(
    const std::string& zenoh_key, CallableConn listener) {
	std::unique_lock<std::mutex> lock(rpc_callback_map_mutex_);
	rpc_callback_map_.insert(std::make_pair(zenoh_key, listener));

	return v1::UStatus();
}

v1::UStatus ZenohUTransport::registerPublishNotificationListener_(
    const std::string& zenoh_key, CallableConn listener) {
	// NOTE: listener is captured by copy here so that it does not go out
	// of scope when this function returns.
	auto on_sample = [this, listener](const zenoh::Sample& sample) mutable {
		listener(sampleToUMessage(sample));
	};

	auto on_drop = []() {};

	auto subscriber = session_.declare_subscriber(
	    zenoh_key, std::move(on_sample), std::move(on_drop));
	{
		std::unique_lock<std::mutex> lock(subscriber_map_mutex_);
		subscriber_map_.emplace(listener, std::move(subscriber));
	}
	return v1::UStatus();
}

v1::UStatus ZenohUTransport::sendRequest_(const std::string& zenoh_key,
                                          const std::string& payload,
                                          const v1::UAttributes& attributes) {
	auto source_str =
	    datamodel::serializer::uri::AsString().serialize(attributes.source());
	CallableConn resp_callback;
	{
		std::unique_lock<std::mutex> lock(rpc_callback_map_mutex_);

		if (auto resp_callback_it = rpc_callback_map_.find(source_str);
		    resp_callback_it == rpc_callback_map_.end()) {
			return uError(v1::UCode::UNAVAILABLE, "failed to find UUID");
		} else {
			resp_callback = resp_callback_it->second;
		}
	}
	auto on_reply = [&](const zenoh::Reply& reply) {
		if (reply.is_ok()) {
			const auto& sample = reply.get_ok();
			resp_callback(sampleToUMessage(sample));
		} else {
			// TODO: error report
			// std::cout << "Received an error :" <<
			// reply.get_err().get_payload().deserialize<std::string>() << "\n";
		}
	};

	auto attachment = uattributesToAttachment(attributes);

	auto on_done = []() {};

	try {
		session_.get(zenoh_key, "", std::move(on_reply), std::move(on_done),
		             {.target = Z_QUERY_TARGET_BEST_MATCHING,
		              .payload = zenoh::Bytes::serialize(payload),
		              .attachment = zenoh::Bytes::serialize(attachment)});
	} catch (const zenoh::ZException& e) {
		return uError(v1::UCode::INTERNAL, e.what());
	}

	return v1::UStatus();
}

v1::UStatus ZenohUTransport::sendResponse_(const std::string& payload,
                                           const v1::UAttributes& attributes) {
	auto reqid_str =
	    datamodel::serializer::uuid::AsString().serialize(attributes.reqid());
	zenoh::Query* query;
	{
		std::unique_lock<std::mutex> lock(query_map_mutex_);
		if (auto query_it = query_map_.find(reqid_str);
		    query_it == query_map_.end()) {
			return uError(v1::UCode::INTERNAL, "query doesn't exist");
		} else {
			query = &query_it->second;
		}
	}

	query->reply(query->get_keyexpr(), payload);

	return v1::UStatus();
}

v1::UStatus ZenohUTransport::sendPublishNotification_(
    const std::string& zenoh_key, const std::string& payload,
    const v1::UAttributes& attributes) {
	auto attachment = uattributesToAttachment(attributes);

	auto priority = mapZenohPriority(attributes.priority());

	try {
		session_.put(zenoh::KeyExpr(zenoh_key),
		             zenoh::Bytes::serialize(payload),
		             {.priority = priority,
		              .encoding = zenoh::Encoding("app/custom"),
		              .attachment = attachment});
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

	switch (attributes.type()) {
		case v1::UMessageType::UMESSAGE_TYPE_PUBLISH: {
			return sendPublishNotification_(zenoh_key, payload, attributes);
		}
		case v1::UMessageType::UMESSAGE_TYPE_NOTIFICATION: {
			return sendPublishNotification_(zenoh_key, payload, attributes);
		}
		case v1::UMessageType::UMESSAGE_TYPE_REQUEST: {
			return sendRequest_(zenoh_key, payload, attributes);
		}
		case v1::UMessageType::UMESSAGE_TYPE_RESPONSE: {
			return sendResponse_(payload, attributes);
		}
		default: {
			return uError(v1::UCode::INVALID_ARGUMENT,
			              "Wrong Message type in v1::UAttributes");
		}
	}
	return v1::UStatus();
}

v1::UStatus ZenohUTransport::registerListenerImpl(
    CallableConn&& listener, const v1::UUri& source_filter,
    std::optional<v1::UUri>&& sink_filter) {
	std::string zenoh_key = toZenohKeyString(getEntityUri().authority_name(),
	                                         source_filter, sink_filter);
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

void ZenohUTransport::cleanupListener(CallableConn listener) {
	{
		std::unique_lock<std::mutex> lock(subscriber_map_mutex_);
		if (subscriber_map_.erase(listener) > 0) {
			return;
		}
	}
	{
		std::unique_lock<std::mutex> lock(queryable_map_mutex_);
		queryable_map_.erase(listener);
	}
}

}  // namespace uprotocol::transport
