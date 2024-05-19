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
#include <optional>

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

	/// @brief Register listener to be called when UMessage is received
	///        for the given URI.
	///
	/// @remarks If this doesn't return OKSTATUS, the public wrapping
	///          version of registerListener() will reset the connection
	///          handle before returning it to the caller.
	///
	/// @param sink_filter UUri for where messages are expected to arrive via
	///                    the underlying transport technology. The callback
	///                    will be called when a message with a matching sink
	/// @param listener shared_ptr to a connected callback object, to be
	///                 called when a message is received.
	/// @param source_filter (Optional) UUri for where messages are expected to
	///                      have been sent from. The callback will only be
	///                      called for messages where the source matches.
	///
	/// @returns * OKSTATUS if the listener was registered successfully.
	///          * FAILSTATUS with the appropriate failure otherwise.
	[[nodiscard]] virtual v1::UStatus registerListenerImpl(
	    const v1::UUri& sink_filter, CallableConn&& listener,
	    std::optional<v1::UUri>&& source_filter) override;

	/// @brief Clean up on listener disconnect.
	///
	/// The transport library can optionally implement this if it needs
	/// to clean up when a callbacks::Connection is dropped.
	///
	/// @note The default implementation does nothing.
	///
	/// @param listener shared_ptr of the Connection that has been broken.
	virtual void cleanupListener(CallableConn listener) override;

private:
};

}  // namespace uprotocol::transport

#endif  // UP_TRANSPORT_ZENOH_CPP_ZENOHUTRANSPORT_H
