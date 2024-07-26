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

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <up-cpp/communication/Publisher.h>
#include <up-cpp/communication/Subscriber.h>
#include <up-cpp/datamodel/serializer/UUri.h>

#include <queue>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace {

using namespace uprotocol;

constexpr std::string_view ZENOH_CONFIG_FILE = BUILD_REALPATH_ZENOH_CONF;

constexpr std::string_view ENTITY_URI_STR = "//test0/10001/1/0";
constexpr std::string_view TOPIC_URI_STR = "//test0/10001/1/8000";

class PublisherSubscriberTest : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	PublisherSubscriberTest() = default;
	~PublisherSubscriberTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

v1::UUri makeUUri(std::string_view authority, uint16_t ue_id,
                  uint16_t ue_instance, uint8_t version, uint16_t resource) {
	v1::UUri uuri;
	uuri.set_authority_name(static_cast<std::string>(authority));
	uuri.set_ue_id((static_cast<uint32_t>(ue_instance) << 16) | ue_id);
	uuri.set_ue_version_major(version);
	uuri.set_resource_id(resource);
	return uuri;
}

v1::UUri makeUUri(std::string_view serialized) {
	return datamodel::serializer::uri::AsString::deserialize(
	    static_cast<std::string>(serialized));
}

std::shared_ptr<transport::UTransport> getTransport(
    const v1::UUri& uuri = makeUUri(ENTITY_URI_STR)) {
	return std::make_shared<transport::ZenohUTransport>(uuri,
	                                                    ZENOH_CONFIG_FILE);
}

using MsgDiff = google::protobuf::util::MessageDifferencer;

// TODO(sashacmc): config generation

TEST_F(PublisherSubscriberTest, SinglePubSingleSub) {
	zenoh::init_logger();

	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI_STR),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};

	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI_STR), std::move(on_rx));

	EXPECT_TRUE(maybe_sub);
	if (!maybe_sub) {
		return;
	}
	auto sub = std::move(maybe_sub).value();

	constexpr size_t num_publish_messages = 25;
	for (auto remaining = num_publish_messages; remaining > 0; --remaining) {
		std::ostringstream message;
		message << "Message number: " << remaining;

		auto result = pub.publish({std::move(message).str(),
		                           v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
		EXPECT_EQ(result.code(), v1::UCode::OK);
	}

	EXPECT_EQ(rx_queue.size(), num_publish_messages);
	EXPECT_NE(sub, nullptr);
	sub.reset();
}

}  // namespace
