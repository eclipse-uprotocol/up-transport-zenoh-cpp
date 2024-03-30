/*
 * Copyright (c) 2024 General Motors GTO LLC
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2024 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <spdlog/spdlog.h>
#include <up-client-zenoh-cpp/client/upZenohClient.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <tuple>
#include <chrono>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::client;

// capture and queue onReceive() for verification by the sender
class MessageCapture : public UListener {
    typedef std::vector<std::uint8_t>    Data;

    std::condition_variable cv;
    std::mutex mtx;
    std::deque<Data>    queue;
public:
    void operator()(UMessage& message)
    {
        using namespace std;

        Data data(message.payload().size());
        auto ptr = message.payload().data();
        copy(ptr, ptr+message.payload().size(), data.data());
        { 
            lock_guard<mutex> lock(mtx);
            queue.emplace_back(data);
            cv.notify_one();
        }
    }

    UStatus onReceive(UMessage& m) const override
    {
        using namespace std;
        // scrape the const-ness off this thing
        const_cast<MessageCapture*>(this)->operator()(m);
        UStatus status;
        status.set_code(UCode::OK);
        return status;
    }

    std::tuple<size_t, Data, std::chrono::time_point<std::chrono::steady_clock>> get(size_t milliseconds_max = 1000)
    {
        using namespace std;
        using namespace std::chrono;

        auto end_time = steady_clock::now() + milliseconds(milliseconds_max);
        size_t count;
        Data data;
        {
            unique_lock<mutex> lock(mtx);
            while ((count = queue.size()) < 1) cv.wait_until(lock, end_time);
            end_time = steady_clock::now();
            data = queue.front();
            queue.pop_front();
        }
        return make_tuple(count, data, end_time);
    }
};

template <typename T>
T byteswap(T input)
{
    T output = 0;
    int cnt = sizeof(T);
    while (cnt-- > 0) {
        output <<= 8;
        output |= input & 0xff;
        input >>= 8;
    }
    return output;
}

// // forward onReceive() to any std::function
// struct Rewrap : public UListener {
//     typedef std::function<UStatus (UMessage&)> Holder;
//     Holder holder;
//     Rewrap(Holder holder) : holder(holder) { }
//     UStatus onReceive(UMessage& m) const override { return holder(m); }
// };

// std::ostream& operator<<(std::ostream& os, const std::vector<std::uint8_t>& data)
// {
//     os << "[ ";
//     for (auto c : data) os << int(c) << ' ';
//     return os << ']';
// }

// UStatus prt_msg(UMessage &message) {
//     using namespace std;
//     (void)message;
//     auto& payload = message.payload();
//     cout << "onReceive payload=" << typeid(payload).name()
//         << " size=" << payload.size()
//         // << " type=" << int(payload.type())
//         << " format=" << payload.format();
//     for (size_t i = 0; i < payload.size(); i++) {
//         cout << ' ' << int(payload.data()[i]);
//     }
//     cout << endl;
//     UStatus status;
//     status.set_code(UCode::OK);
//     return status;
// }

struct TestPubSub : public ::testing::Test {
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {}

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {}
};

TEST_F(TestPubSub, pub) {
    using namespace std::chrono;
    auto transport = UpZenohClient::instance();

    UUri uuri = LongUriSerializer::deserialize("/test.app/1/milliseconds");
    MessageCapture callback;
    UStatus listen_status = transport->registerListener(uuri, callback);
    EXPECT_EQ(UCode::OK, listen_status.code());

    auto builder = UAttributesBuilder::publish(uuri, UPriority::UPRIORITY_CS0);
    UAttributes attributes = builder.build();

    const size_t cnt = 10;
    for (size_t out_data = 0; out_data < cnt; out_data++) {
        decltype(out_data) in_data;
        UPayload payload((uint8_t*)&out_data, sizeof(out_data), UPayloadType::VALUE);
        UMessage message(payload, attributes);
        auto send_time = steady_clock::now();
        UStatus send_status = transport->send(message);
        EXPECT_EQ(UCode::OK, send_status.code());
        auto [ sz, in_vector, cap_time ] = callback.get(100);
        EXPECT_EQ(1, sz);
        EXPECT_EQ(sizeof(out_data), in_vector.size());
        copy(in_vector.data(), in_vector.data()+sizeof(in_data), &in_data);
        EXPECT_EQ(out_data == in_data, true);
        cout << "us = " << duration_cast<microseconds>(cap_time - send_time).count() << endl;
    }
    listen_status = transport->unregisterListener(uuri, callback);
    EXPECT_EQ(UCode::OK, listen_status.code());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}