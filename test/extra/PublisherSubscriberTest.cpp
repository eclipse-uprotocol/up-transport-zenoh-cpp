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

#include <queue>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace {
constexpr size_t num_publish_messages = 25;

using namespace uprotocol;

constexpr std::string_view ZENOH_CONFIG_FILE = BUILD_REALPATH_ZENOH_CONF;

constexpr int ENTITY_URI = 0;
constexpr int TOPIC_URI = 0x8000;
constexpr int TOPIC_URI2 = 0x8001;

class PublisherSubscriberTest : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	PublisherSubscriberTest() { zenoh::init_logger(); }
	~PublisherSubscriberTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

v1::UUri makeUUri(uint16_t resource_id) {
	v1::UUri uuri;
	uuri.set_authority_name(static_cast<std::string>("test0"));
	uuri.set_ue_id((0x10001));
	uuri.set_ue_version_major(1);
	uuri.set_resource_id(resource_id);
	return uuri;
}

std::shared_ptr<transport::UTransport> getTransport(
    const v1::UUri& uuri = makeUUri(ENTITY_URI)) {
	return std::make_shared<transport::ZenohUTransport>(uuri,
	                                                    ZENOH_CONFIG_FILE);
}

using MsgDiff = google::protobuf::util::MessageDifferencer;

// TODO(sashacmc): config generation

TEST_F(PublisherSubscriberTest, SinglePubSingleSub) {
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};

	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	if (maybe_sub) {
		for (auto remaining = num_publish_messages; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_publish_messages);
}

// Single publisher, multiple subscribers (2) on the same topic.
TEST_F(PublisherSubscriberTest, SinglePubMultipleSub) {
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};

	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = num_publish_messages; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_publish_messages * 2);
}

// Single publisher, two subscribers on different topics
TEST_F(PublisherSubscriberTest, SinglePubMultipleSubDifferentTopics) {
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};

	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	// subscribe to a different topic (non-existent topic)
	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI2), std::move(on_rx));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = num_publish_messages; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_publish_messages);
}

// Two publishers, two subscribers, two topics
TEST_F(PublisherSubscriberTest, MultiplePubMultipleSubDifferentTopics) {
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	communication::Publisher pub2(transport, makeUUri(TOPIC_URI2),
	                              v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};

	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI2), std::move(on_rx));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = num_publish_messages; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);

			auto result2 =
			    pub2.publish({std::move(message).str(),
			                  v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result2.code(), v1::UCode::OK);
		}
	}

	EXPECT_EQ(rx_queue.size(), num_publish_messages * 2);
}

}  // namespace
