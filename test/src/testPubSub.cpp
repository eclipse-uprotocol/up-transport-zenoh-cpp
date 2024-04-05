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
// #include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <up-cpp/uri/serializer/MicroUriSerializer.h>
#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>
#include <tuple>
#include <chrono>
#include <random>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::client;

// capture and queue onReceive() for verification by the sender

template <bool INTERPROC> 
class MessageCapture : public UListener {
public:
    typedef std::vector<std::uint8_t>    Data;
private:
    std::condition_variable cv;
    std::mutex mtx;
    std::deque<Data>    queue;
    int sp[2];
public:
    MessageCapture()
    {
        if constexpr (INTERPROC) {
            socketpair(AF_UNIX, SOCK_DGRAM, 0, sp);
        }
    }

    ~MessageCapture()
    {
        if constexpr (INTERPROC) {
            close(sp[0]);
            close(sp[1]);
        }
    }

    void operator()(UMessage& message)
    {
        using namespace std;

        if constexpr (INTERPROC) {
            send(sp[0], message.payload().data(), message.payload().size(), 0);
        }
        else {
            Data data(message.payload().size());
            auto ptr = message.payload().data();
            copy(ptr, ptr+message.payload().size(), data.data());
            { 
                lock_guard<mutex> lock(mtx);
                queue.emplace_back(data);
                cv.notify_one();
            }
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
        if constexpr (INTERPROC) {
            Data data(2048);
            struct pollfd fds[1];
            fds[0].fd = sp[1];
            fds[0].events = POLLIN;
            fds[0].revents = 0;
            auto ret = poll(fds, 1, milliseconds_max);
            if (ret <= 0) return make_tuple(0, Data(), end_time);
            ret = recv(sp[1], data.data(), 2048, 0);
            if (ret <= 0) return make_tuple(0, Data(), end_time);
            end_time = steady_clock::now();
            data.resize(ret);
            return make_tuple(1, data, end_time);
        }
        else {
            size_t count;
            Data data;
            {
                unique_lock<mutex> lock(mtx);
                while ((count = queue.size()) < 1) cv.wait_until(lock, end_time);
                if (count > 0) {
                    end_time = steady_clock::now();
                    data = queue.front();
                    queue.pop_front();
                    return make_tuple(count, data, end_time);
                }
            }
            return make_tuple(0, data, end_time);
        }
    }
};

static random_device rand_dev;
static default_random_engine rand_eng(rand_dev());

static std::vector<uint8_t> get_random_vec(size_t len)
{
    static uniform_int_distribution<> rand_dist(0, 0xff);
    vector<uint8_t> results(len);
    for (size_t i = 0; i < len; i++) results[i] = rand_dist(rand_eng);
    return results;
}

static size_t get_random_length()
{
    static uniform_int_distribution<> rand_dist(1, 1400);
    return rand_dist(rand_eng);
}

struct TestPubSub : public ::testing::Test {
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {}

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {}
};

TEST_F(TestPubSub, interprocess) {
    using namespace std::chrono;

    auto uuri = BuildUUri()
                      .setAutority(BuildUAuthority().build())
                      .setEntity(BuildUEntity()
                              .setMajorVersion(1)
                              .setId(1)
                              .build())
                      .setResource(BuildUResource()
                              .setID(1)
                              .build())
                      .build();

    MessageCapture<true> callback;
    auto child_pid = fork();
    if (child_pid == 0) {
        auto transport = upZenohClient::instance();
        UStatus listen_status = transport->registerListener(uuri, callback);
        if (UCode::OK != listen_status.code()) {
            cerr << "child process listen failed" << endl;
            exit(-1);
        }
        sleep(100000);
        exit(0);
    }
    else {
        sleep(1);
        auto transport = upZenohClient::instance();

        auto builder = UAttributesBuilder::publish(uuri, UPriority::UPRIORITY_CS0);
        UAttributes attributes = builder.build();

        const size_t cnt = 1000;
        for (size_t i = 0; i < cnt; i++) {
            auto out_data = get_random_vec(get_random_length());
            // cout << "test length = " << out_data.size() << endl;
            UPayload payload((uint8_t*)out_data.data(), out_data.size(), UPayloadType::VALUE);
            UMessage message(payload, attributes);
            auto send_time = steady_clock::now();
            UStatus send_status = transport->send(message);
            EXPECT_EQ(UCode::OK, send_status.code());
            auto [ sz, in_data, cap_time ] = callback.get(2000);
            EXPECT_EQ(1, sz);
            EXPECT_EQ(out_data.size(), in_data.size());
            EXPECT_EQ(out_data == in_data, true);
            // cout << "us = " << duration_cast<microseconds>(cap_time - send_time).count() << endl;
        }
        // listen_status = transport->unregisterListener(uuri, callback);
        // EXPECT_EQ(UCode::OK, listen_status.code());
        kill(child_pid, SIGINT);
    }
}

TEST_F(TestPubSub, interthread) {
    using namespace std::chrono;
    auto transport = upZenohClient::instance();

    auto uuri = BuildUUri()
                      .setAutority(BuildUAuthority().build())
                      .setEntity(BuildUEntity()
                              .setMajorVersion(1)
                              .setId(1)
                              .build())
                      .setResource(BuildUResource()
                              .setID(1)
                              .build())
                      .build();

    MessageCapture<false> callback;
    UStatus listen_status = transport->registerListener(uuri, callback);
    EXPECT_EQ(UCode::OK, listen_status.code());

    auto builder = UAttributesBuilder::publish(uuri, UPriority::UPRIORITY_CS0);
    UAttributes attributes = builder.build();

    const size_t cnt = 1000;
    for (size_t i = 0; i < cnt; i++) {
        auto out_data = get_random_vec(get_random_length());
        // cout << "test length = " << out_data.size() << endl;
        UPayload payload((uint8_t*)out_data.data(), out_data.size(), UPayloadType::VALUE);
        UMessage message(payload, attributes);
        auto send_time = steady_clock::now();
        UStatus send_status = transport->send(message);
        EXPECT_EQ(UCode::OK, send_status.code());
        auto [ sz, in_data, cap_time ] = callback.get(2000);
        EXPECT_EQ(1, sz);
        EXPECT_EQ(out_data.size(), in_data.size());
        EXPECT_EQ(out_data == in_data, true);
        // cout << "us = " << duration_cast<microseconds>(cap_time - send_time).count() << endl;
    }
    listen_status = transport->unregisterListener(uuri, callback);
    EXPECT_EQ(UCode::OK, listen_status.code());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}