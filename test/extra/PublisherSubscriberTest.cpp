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
#include <up-cpp/communication/Publisher.h>
#include <up-cpp/communication/Subscriber.h>

#include <queue>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

constexpr size_t NUM_PUBLISH_MESSAGES = 25;

namespace uprotocol {

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
#ifdef ZENOHCXX_ZENOHPICO
	PublisherSubscriberTest() {}
#else
	PublisherSubscriberTest() { zenoh::init_log_from_env_or("error"); }
#endif

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}

public:
	~PublisherSubscriberTest() override = default;
};

v1::UUri makeUUri(uint16_t resource_id) {
	constexpr uint32_t DEFAULT_UE_ID = 0x10001;
	v1::UUri uuri;
	uuri.set_authority_name(static_cast<std::string>("test0"));
	uuri.set_ue_id((DEFAULT_UE_ID));
	uuri.set_ue_version_major(1);
	uuri.set_resource_id(resource_id);
	return uuri;
}

std::shared_ptr<transport::UTransport> getTransport(
    const v1::UUri& uuri = makeUUri(ENTITY_URI)) {
#ifdef ZENOHCXX_ZENOHPICO
	return std::make_shared<transport::ZenohUTransport>(uuri);
#else
	return std::make_shared<transport::ZenohUTransport>(uuri,
	                                                    ZENOH_CONFIG_FILE);
#endif
}

// ValidateMessages
// Validates the messages received by the subscriber.  Every message has a
// sequencial message number, and the sum of all the numbers should be equal to
// (num_messages * (num_messages + 1)) / 2.  Additionally, each message should
// match the expected prefix.
void ValidateMessages(std::queue<v1::UMessage>& rx_queue, size_t num_messages,
                      const std::string& prefix) {
	EXPECT_EQ(rx_queue.size(), num_messages);

	int sum = 0;
	while (!rx_queue.empty()) {
		auto message = rx_queue.front();
		rx_queue.pop();

		auto pos = message.payload().find(prefix);
		int num = std::stoi(message.payload().substr(prefix.size()));
		sum += num;
		EXPECT_NE(pos, std::string::npos);
	}
	EXPECT_EQ(sum, (num_messages * (num_messages + 1)) / 2);
}

// TODO(sashacmc): config generation

TEST_F(PublisherSubscriberTest, SinglePubSingleSub) {  // NOLINT
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
		for (auto remaining = NUM_PUBLISH_MESSAGES; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	ValidateMessages(rx_queue, NUM_PUBLISH_MESSAGES, "Message number: ");
}

// Single publisher, multiple subscribers (2) on the same topic.
TEST_F(PublisherSubscriberTest, SinglePubMultipleSub) {  // NOLINT
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	// First subscriber and queue
	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};
	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	// Second subscriber and queue
	std::mutex rx_queue_mtx2;
	std::queue<v1::UMessage> rx_queue2;
	auto on_rx2 = [&rx_queue_mtx2, &rx_queue2](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx2);
		rx_queue2.push(message);
	};
	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx2));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = NUM_PUBLISH_MESSAGES; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	ValidateMessages(rx_queue, NUM_PUBLISH_MESSAGES, "Message number: ");
	ValidateMessages(rx_queue2, NUM_PUBLISH_MESSAGES, "Message number: ");
}

// Single publisher, two subscribers on different topics
TEST_F(PublisherSubscriberTest,  // NOLINT
       SinglePubMultipleSubDifferentTopics) {
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
	auto on_rx2 = [](const v1::UMessage& /*message*/) { FAIL(); };
	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI2), std::move(on_rx2));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = NUM_PUBLISH_MESSAGES; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Message number: " << remaining;

			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);
		}
	}

	ValidateMessages(rx_queue, NUM_PUBLISH_MESSAGES, "Message number: ");
}

// Two publishers, two subscribers, two topics
TEST_F(PublisherSubscriberTest,  // NOLINT
       MultiplePubMultipleSubDifferentTopics) {
	auto transport = getTransport();

	communication::Publisher pub(transport, makeUUri(TOPIC_URI),
	                             v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
	communication::Publisher pub2(transport, makeUUri(TOPIC_URI2),
	                              v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	// first subscriber and queue
	std::mutex rx_queue_mtx;
	std::queue<v1::UMessage> rx_queue;
	auto on_rx = [&rx_queue_mtx, &rx_queue](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx);
		rx_queue.push(message);
	};
	auto maybe_sub = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI), std::move(on_rx));
	EXPECT_TRUE(maybe_sub);

	// second subscriber and queue
	std::mutex rx_queue_mtx2;
	std::queue<v1::UMessage> rx_queue2;
	auto on_rx2 = [&rx_queue_mtx2, &rx_queue2](const v1::UMessage& message) {
		std::lock_guard lock(rx_queue_mtx2);
		rx_queue2.push(message);
	};
	auto maybe_sub2 = communication::Subscriber::subscribe(
	    transport, makeUUri(TOPIC_URI2), std::move(on_rx2));
	EXPECT_TRUE(maybe_sub2);

	if (maybe_sub && maybe_sub2) {
		for (auto remaining = NUM_PUBLISH_MESSAGES; remaining > 0;
		     --remaining) {
			std::ostringstream message;
			message << "Pub 1 - Message number: " << remaining;
			auto result =
			    pub.publish({std::move(message).str(),
			                 v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result.code(), v1::UCode::OK);

			std::ostringstream message2;
			message2 << "Pub 2 - Message number: " << remaining;
			auto result2 =
			    pub2.publish({std::move(message2).str(),
			                  v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT});
			EXPECT_EQ(result2.code(), v1::UCode::OK);
		}
	}

	ValidateMessages(rx_queue, NUM_PUBLISH_MESSAGES,
	                 "Pub 1 - Message number: ");
	ValidateMessages(rx_queue2, NUM_PUBLISH_MESSAGES,
	                 "Pub 2 - Message number: ");
}

}  // namespace uprotocol
