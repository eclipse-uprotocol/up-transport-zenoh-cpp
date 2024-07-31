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

#include <iostream>

using namespace std::chrono_literals;

namespace {

using namespace uprotocol::v1;
using uprotocol::communication::RpcClient;
using uprotocol::communication::RpcServer;

constexpr std::string_view ZENOH_CONFIG_FILE = BUILD_REALPATH_ZENOH_CONF;

struct MyUUri {
	std::string auth = "";
	uint32_t ue_id = 0x8000;
	uint32_t ue_version_major = 1;
	uint32_t resource_id = 1;

	operator uprotocol::v1::UUri() const {
		UUri ret;
		ret.set_authority_name(auth);
		ret.set_ue_id(ue_id);
		ret.set_ue_version_major(ue_version_major);
		ret.set_resource_id(resource_id);
		return ret;
	}

	std::string to_string() const {
		return std::string("<< ") + UUri(*this).ShortDebugString() + " >>";
	}
};

class RpcClientServerTest : public testing::Test {
protected:
	MyUUri rpc_service_uuri_{"me_authority", 65538, 1, 32600};
	MyUUri ident_{"me_authority", 65538, 1, 0};

	using Transport = uprotocol::transport::ZenohUTransport;
	std::shared_ptr<Transport> transport_;

	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {
		transport_ = std::make_shared<Transport>(ident_, ZENOH_CONFIG_FILE);
		EXPECT_NE(nullptr, transport_);
	}

	void TearDown() override { transport_ = nullptr; }

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	RpcClientServerTest() = default;
	~RpcClientServerTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

TEST_F(RpcClientServerTest, SimpleRoundTrip) {
	using namespace std;

	string client_request{"RPC Request"};
	uprotocol::datamodel::builder::Payload client_request_payload(
	    client_request, UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	bool client_called = false;
	UMessage client_capture;

	bool server_called = false;
	UMessage server_capture;
	string server_response{"RPC Response"};
	uprotocol::datamodel::builder::Payload server_response_payload(
	    server_response, UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	auto serverOrStatus = RpcServer::create(
	    transport_, rpc_service_uuri_,
	    [this, &server_called, &server_capture,
	     &server_response_payload](const UMessage& message) {
		    server_called = true;
		    server_capture = message;
		    return server_response_payload;
	    },
	    UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	ASSERT_TRUE(serverOrStatus.has_value());
	ASSERT_NE(serverOrStatus.value(), nullptr);

	auto client = RpcClient(transport_, rpc_service_uuri_,
	                        UPriority::UPRIORITY_CS4, 1000ms);

	uprotocol::communication::RpcClient::InvokeHandle client_handle;
	EXPECT_NO_THROW(
	    client_handle = client.invokeMethod(
	        std::move(client_request_payload),
	        [this, &client_called, &client_capture](auto maybe_response) {
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

}  // namespace
