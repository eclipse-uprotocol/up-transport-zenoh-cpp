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

#include <gtest/gtest.h>
#include <up-cpp/communication/RpcClient.h>
#include <up-cpp/communication/RpcServer.h>
#include <up-cpp/datamodel/builder/Payload.h>
#include <up-cpp/datamodel/builder/Uuid.h>
#include <up-cpp/datamodel/serializer/UUri.h>
#include <up-cpp/datamodel/serializer/Uuid.h>
#include <up-transport-zenoh-cpp/ZenohUTransport.h>
#include <uprotocol/v1/uri.pb.h>

#include <utility>

using namespace std::chrono_literals;

namespace uprotocol::v1 {
using uprotocol::communication::RpcClient;
using uprotocol::communication::RpcServer;

constexpr std::string_view ZENOH_CONFIG_FILE = BUILD_REALPATH_ZENOH_CONF;

struct UeDetails {
	uint32_t ue_id;
	uint32_t ue_version_major;
};

struct MyUUri {
	static constexpr uint32_t DEFAULT_UE_ID = 0x8000;

public:
	MyUUri(std::string auth_val, UeDetails ue_details, uint32_t resource_id_val)
	    : auth(std::move(auth_val)),
	      ue_id(ue_details.ue_id),
	      ue_version_major(ue_details.ue_version_major),
	      resource_id(resource_id_val) {}

	void set_auth(const std::string& auth_val) { auth = auth_val; }
	[[nodiscard]] const std::string& get_auth() const { return auth; }

	void set_ue_details(UeDetails ue_details) {
		ue_id = ue_details.ue_id;
		ue_version_major = ue_details.ue_version_major;
	}

	[[nodiscard]] uint32_t get_ue_id() const { return ue_id; }
	[[nodiscard]] uint32_t get_ue_version_major() const {
		return ue_version_major;
	}

	void set_resource_id(uint32_t resource) { resource_id = resource; }
	[[nodiscard]] uint32_t get_resource_id() const { return resource_id; }

	explicit operator uprotocol::v1::UUri() const {
		UUri ret;
		ret.set_authority_name(auth);
		ret.set_ue_id(ue_id);
		ret.set_ue_version_major(ue_version_major);
		ret.set_resource_id(resource_id);
		return ret;
	}

	[[nodiscard]] std::string to_string() const {
		return std::string("<< ") + UUri(*this).ShortDebugString() + " >>";
	}

private:
	std::string auth;
	uint32_t ue_id = DEFAULT_UE_ID;
	uint32_t ue_version_major = 1;
	uint32_t resource_id = 1;
};

class RpcClientServerTest : public testing::Test {
protected:
	using Transport = uprotocol::transport::ZenohUTransport;
	std::shared_ptr<Transport> transport_ = nullptr;  // NOLINT

	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {
		const MyUUri ident{"me_authority", {65538, 1}, 0};
#ifdef ZENOHCXX_ZENOHPICO
		transport_ = std::make_shared<Transport>(static_cast<v1::UUri>(ident));
#else
		transport_ = std::make_shared<Transport>(static_cast<v1::UUri>(ident),
		                                         ZENOH_CONFIG_FILE);
#endif
		EXPECT_NE(nullptr, transport_);
	}

	void TearDown() override { transport_ = nullptr; }

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	RpcClientServerTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}

public:
	~RpcClientServerTest() override = default;
};

TEST_F(RpcClientServerTest, SimpleRoundTrip) {  // NOLINT
	const MyUUri rpc_service_uuri{"me_authority", {65538, 1}, 32600};
	std::string client_request{"RPC Request"};  // NOLINT
	uprotocol::datamodel::builder::Payload client_request_payload(
	    client_request, UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	bool client_called = false;
	UMessage client_capture;  // NOLINT

	bool server_called = false;
	UMessage server_capture;                      // NOLINT
	std::string server_response{"RPC Response"};  // NOLINT
	uprotocol::datamodel::builder::Payload server_response_payload(
	    server_response, UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	auto server_or_status = RpcServer::create(
	    transport_, v1::UUri(rpc_service_uuri),
	    [&server_called, &server_capture,
	     &server_response_payload](const UMessage& message) {
		    server_called = true;
		    server_capture = message;
		    return server_response_payload;
	    },
	    UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	ASSERT_TRUE(server_or_status.has_value());
	ASSERT_NE(server_or_status.value(), nullptr);

	auto client = RpcClient(transport_, UPriority::UPRIORITY_CS4, 1000ms);

	uprotocol::communication::RpcClient::InvokeHandle client_handle;  // NOLINT
	EXPECT_NO_THROW(                                                  // NOLINT
	    client_handle = client.invokeMethod(
	        v1::UUri(rpc_service_uuri), std::move(client_request_payload),
	        [&client_called, &client_capture](const auto& maybe_response) {
		        client_called = true;
		        if (maybe_response.has_value()) {
			        client_capture = maybe_response.value();
		        }
	        }));

	EXPECT_TRUE(server_called);
	EXPECT_EQ(client_request, server_capture.payload());
	EXPECT_TRUE(client_called);
	EXPECT_EQ(server_response, client_capture.payload());
}

}  // namespace uprotocol::v1
