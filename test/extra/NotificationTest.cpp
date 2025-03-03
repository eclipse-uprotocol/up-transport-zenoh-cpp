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
#include <up-cpp/communication/NotificationSink.h>
#include <up-cpp/communication/NotificationSource.h>

#include <queue>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace {

using namespace uprotocol;

constexpr std::string_view ZENOH_CONFIG_FILE = BUILD_REALPATH_ZENOH_CONF;

class NotificationTest : public testing::Test {
protected:
	// Run once per TEST_F
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	NotificationTest() = default;
	~NotificationTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

v1::UUri getUUri(uint16_t resource) {
	v1::UUri uuri;
	uuri.set_authority_name(static_cast<std::string>("test0"));
	uuri.set_ue_id(0x10001);
	uuri.set_ue_version_major(1);
	uuri.set_resource_id(resource);
	return uuri;
}

std::shared_ptr<transport::UTransport> getTransport(
    const v1::UUri& uuri = getUUri(0)) {
	return std::make_shared<transport::ZenohUTransport>(uuri,
	                                                    ZENOH_CONFIG_FILE);
}

TEST_F(NotificationTest, BasicNotificationTestWithPayload) {
	zenoh::init_log_from_env_or("error");

	auto transport = getTransport();
	auto source = getUUri(0x8000);
	auto sink = getUUri(0);
	auto source_filter = source;
	constexpr int num_messages = 25;

	auto notificationSource = communication::NotificationSource(
	    transport, std::move(source), std::move(sink),
	    v1::UPAYLOAD_FORMAT_TEXT);

	// Create the intended sink for the notifications
	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard<std::mutex> lock(rx_queue_mtx);
		rx_queue.push(std::move(message));
	};
	auto maybe_sink = communication::NotificationSink::create(
	    transport, std::move(on_rx), std::move(source_filter));
	EXPECT_TRUE(maybe_sink.has_value());

	// Create a second sink with a different source filter to verify messages
	// arrive at the right sink
	auto source_filter2 = getUUri(0x8001);
	auto on_rx2 = [](const v1::UMessage& message) { FAIL(); };
	auto maybe_sink2 = communication::NotificationSink::create(
	    transport, std::move(on_rx2), std::move(source_filter2));
	EXPECT_TRUE(maybe_sink2.has_value());

	// Send the notification messages
	if (maybe_sink.has_value()) {
		for (auto remaining = num_messages; remaining > 0; --remaining) {
			std::string message = "Hello, world!";
			datamodel::builder::Payload payload(message,
			                                    v1::UPAYLOAD_FORMAT_TEXT);
			auto status = notificationSource.notify(std::move(payload));
			EXPECT_EQ(status.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_messages);
}

TEST_F(NotificationTest, BasicNotificationTestWithoutPayload) {
	zenoh::init_log_from_env_or("error");

	auto transport = getTransport();
	auto source = getUUri(0x8000);
	auto sink = getUUri(0);
	auto source_filter = source;
	constexpr int num_messages = 25;

	auto notificationSource = communication::NotificationSource(
	    transport, std::move(source), std::move(sink));

	// Create the intended sink for the notifications
	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard<std::mutex> lock(rx_queue_mtx);
		rx_queue.push(std::move(message));
	};
	auto maybe_sink = communication::NotificationSink::create(
	    transport, std::move(on_rx), std::move(source_filter));
	EXPECT_TRUE(maybe_sink.has_value());

	// Create a second sink with a different source filter to verify messages
	// arrive at the right sink
	auto source_filter2 = getUUri(0x8001);
	auto on_rx2 = [](const v1::UMessage& message) { FAIL(); };
	auto maybe_sink2 = communication::NotificationSink::create(
	    transport, std::move(on_rx2), std::move(source_filter2));
	EXPECT_TRUE(maybe_sink2.has_value());

	// Send the notification messages
	if (maybe_sink.has_value()) {
		for (auto remaining = num_messages; remaining > 0; --remaining) {
			auto status = notificationSource.notify();
			EXPECT_EQ(status.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_messages);
}

}  // namespace
