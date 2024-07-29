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

#ifndef UP_TRANSPORT_ZENOH_CPP_ZENOHUTRANSPORT_H
#define UP_TRANSPORT_ZENOH_CPP_ZENOHUTRANSPORT_H

#include <up-cpp/transport/UTransport.h>

#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>

#define ZENOHCXX_ZENOHC
#include <zenoh.hxx>

namespace zenohc {

class OwnedQuery {
public:
	OwnedQuery(const z_query_t& query) : _query(z_query_clone(&query)) {}

	OwnedQuery(const OwnedQuery&) = delete;
	OwnedQuery& operator=(const OwnedQuery&) = delete;

	~OwnedQuery() { z_drop(&_query); }

	Query loan() const { return z_loan(_query); }
	bool check() const { return z_check(_query); }

private:
	z_owned_query_t _query;
};

using OwnedQueryPtr = std::shared_ptr<OwnedQuery>;

}  // namespace zenohc

namespace uprotocol::transport {

/// @brief Zenoh implementation of UTransport
///
/// This implementation must meet the following requirements:
///
/// * [MUST] An implementation MUST support multiple simultaneous instantiatons
///          from within the same application.
///
/// * [MAY]  An implimentation MAY require that each new instance within an
///          application have a unique configuration from existing instances.
///
/// * [MUST] An implementation MUST allow for multiple users of the same
///          instance.
///
/// * [MUST] An implementation MUST be thread-safe.
///
/// * [MUST] Throw an exception if the transport fails to initialize or the
///          configuration is invalid.
struct ZenohUTransport : public UTransport {
	/// @brief Constructor
	///
	/// @param defaultUri Default Authority and Entity (as a UUri) for
	///                   clients using this transport instance.
	/// @param configFile Path to a configuration file containing the Zenoh
	///                   transport configuration.
	ZenohUTransport(const v1::UUri& defaultUri,
	                const std::filesystem::path& configFile);

	virtual ~ZenohUTransport() = default;

protected:
	/// @brief Send a message.
	///
	/// @param message UMessage to be sent.
	///
	/// @returns * OKSTATUS if the payload has been successfully
	///            sent (ACK'ed)
	///          * FAILSTATUS with the appropriate failure otherwise.
	[[nodiscard]] virtual v1::UStatus sendImpl(
	    const v1::UMessage& message) override;

	/// @brief Represents the callable end of a callback connection.
	using CallableConn = typename UTransport::CallableConn;
	using UuriKey = std::string;

	/// @brief Register listener to be called when UMessage is received
	///        for the given URI.
	///
	/// @remarks If this doesn't return OKSTATUS, the public wrapping
	///          version of registerListener() will reset the connection
	///          handle before returning it to the caller.
	///
	/// @see up-cpp for additional details
	///
	/// @returns * OKSTATUS if the listener was registered successfully.
	///          * FAILSTATUS with the appropriate failure otherwise.
	[[nodiscard]] virtual v1::UStatus registerListenerImpl(
	    CallableConn&& listener, const v1::UUri& source_filter,
	    std::optional<v1::UUri>&& sink_filter) override;

	/// @brief Clean up on listener disconnect.
	///
	/// The transport library can optionally implement this if it needs
	/// to clean up when a callbacks::Connection is dropped.
	///
	/// @note The default implementation does nothing.
	///
	/// @param listener shared_ptr of the Connection that has been broken.
	virtual void cleanupListener(CallableConn listener) override;

	static std::string toZenohKeyString(
	    const std::string& default_authority_name, const v1::UUri& source,
	    const std::optional<v1::UUri>& sink);

private:
	static v1::UStatus uError(v1::UCode code, std::string_view message);

	static std::vector<std::pair<std::string, std::string>>
	uattributesToAttachment(const v1::UAttributes& attributes);

	static v1::UAttributes attachmentToUAttributes(
	    const zenoh::AttachmentView& attachment);

	static zenoh::Priority mapZenohPriority(v1::UPriority upriority);

	static v1::UMessage sampleToUMessage(const zenoh::Sample& sample);

	v1::UStatus registerRequestListener_(const std::string& zenoh_key,
	                                     CallableConn listener);

	v1::UStatus registerResponseListener_(const std::string& zenoh_key,
	                                      CallableConn listener);

	v1::UStatus registerPublishNotificationListener_(
	    const std::string& zenoh_key, CallableConn listener);

	v1::UStatus sendRequest_(const std::string& zenoh_key,
	                         const std::string& payload,
	                         const v1::UAttributes& attributes);

	v1::UStatus sendResponse_(const std::string& payload,
	                          const v1::UAttributes& attributes);

	v1::UStatus sendPublishNotification_(const std::string& zenoh_key,
	                                     const std::string& payload,
	                                     const v1::UAttributes& attributes);

	zenoh::Session session_;

	std::map<UuriKey, CallableConn> rpc_callback_map_;
	std::mutex rpc_callback_map_mutex_;

	std::map<CallableConn, zenoh::Subscriber> subscriber_map_;
	std::mutex subscriber_map_mutex_;

	std::map<CallableConn, zenoh::Queryable> queryable_map_;
	std::mutex queryable_map_mutex_;

	std::map<std::string, zenoh::OwnedQueryPtr> query_map_;
	std::mutex query_map_mutex_;
};

}  // namespace uprotocol::transport

#endif  // UP_TRANSPORT_ZENOH_CPP_ZENOHUTRANSPORT_H
