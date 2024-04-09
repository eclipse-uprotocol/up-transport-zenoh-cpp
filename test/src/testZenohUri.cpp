// Copyright (c) 2024 General Motors GTO LLC
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
// SPDX-FileType: SOURCE
// SPDX-FileCopyrightText: 2024 General Motors GTO LLC
// SPDX-License-Identifier: Apache-2.0

#include <spdlog/spdlog.h>
#include <up-cpp/uri/builder/BuildUUri.h>
#include <up-cpp/uri/builder/BuildEntity.h>
#include <up-cpp/uri/builder/BuildUResource.h>
#include <up-cpp/uri/builder/BuildUAuthority.h>
#include <up-client-zenoh-cpp/uri/zenohUri.h>
#include <gtest/gtest.h>

using namespace uprotocol::v1;
using namespace uprotocol::uri;

TEST(TestZenohUri, UriToZenohKey) {
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("body.access")
                    .setMajorVersion(1)
                    .setId(1234)
                    .build())
            .setResource(BuildUResource()
                    .setName("door")
                    .setInstance("front_left")
                    .setMessage("Door")
                    .setID(5678)
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upl/0100162e04d20100");
    }
    {
        auto uuri = BuildUUri()
            .setAutority(BuildUAuthority()
                    .setIp("10.16.7.4")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/0a100704/**");
    }
    {
        auto uuri = BuildUUri()
            .setAutority(BuildUAuthority()
                    .setIp("fe00:0:0:98::1")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/fe000000000000980000000000000001/**");
    }
    {
        auto uuri = BuildUUri()
            .setAutority(BuildUAuthority()
                    .setId({1, 2, 3, 10, 11, 12})
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/060102030a0b0c/**");
    }
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("something")
                    .setMajorVersion(2)
                    .setId(15)
                    .build())
            .setResource(BuildUResource()
                    .setName("some_other")
                    .setInstance("thing")
                    .setMessage("so")
                    .setID(127)
                    .build())
            .setAutority(BuildUAuthority()
                    .setIp("1.1.1.1")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/01010101/0101007f000f0200");
    }
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("coffee")
                    .setMajorVersion(0xee)
                    .setId(0xc0ff)
                    .build())
            .setResource(BuildUResource()
                    .setName("cup")
                    .setInstance("brewed_coffee")
                    .setMessage("add_milk")
                    .setID(0x99)
                    .build())
            .setAutority(BuildUAuthority()
                    .setIp("2001::dead:beef")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/200100000000000000000000deadbeef/01020099c0ffee00");
    }
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("coffee")
                    .setMajorVersion(0xee)
                    .setId(0xc0ff)
                    .build())
            .setResource(BuildUResource()
                    .setName("cup")
                    .setInstance("brewed_coffee")
                    .setMessage("add_milk")
                    .setID(0x99)
                    .build())
            .setAutority(BuildUAuthority()
                    .setId({'H', 'e', 'l', 'l', 'o'})
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);

        EXPECT_EQ(zKey, "upr/0548656c6c6f/01030099c0ffee00");
    }
}

TEST(TestZenohUri, EmptyUUri) {
    auto uuri = BuildUUri().build();
    auto zKey = toZenohKeyString(uuri);
    EXPECT_EQ(zKey, "");
}

TEST(TestZenohUri, EmptyUEntity) {
    auto uuri = BuildUUri()
        .setResource(BuildUResource()
                .setName("cup")
                .setInstance("brewed_coffee")
                .setMessage("add_milk")
                .setID(0x99)
                .build())
        .setAutority(BuildUAuthority()
                .setId({'H', 'e', 'l', 'l', 'o'})
                .build())
        .build();
    auto zKey = toZenohKeyString(uuri);
    EXPECT_EQ(zKey, "");
}

TEST(TestZenohUri, EmptyUResource) {
    auto uuri = BuildUUri()
        .setEntity(BuildUEntity()
                .setName("coffee")
                .setMajorVersion(0xee)
                .setId(0xc0ff)
                .build())
        .setAutority(BuildUAuthority()
                .setId({'H', 'e', 'l', 'l', 'o'})
                .build())
        .build();
    auto zKey = toZenohKeyString(uuri);
    EXPECT_EQ(zKey, "");
}

TEST(TestZenohUri, LongUri) {
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("coffee")
                    .setMajorVersion(0xee)
                    .build())
            .setResource(BuildUResource()
                    .setName("cup")
                    .setInstance("brewed_coffee")
                    .setMessage("add_milk")
                    .setID(0x99)
                    .build())
            .setAutority(BuildUAuthority()
                    .setIp("2001::dead:beef")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);
        EXPECT_EQ(zKey, "");
    }
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("coffee")
                    .setMajorVersion(0xee)
                    .setId(0xc0ff)
                    .build())
            .setResource(BuildUResource()
                    .setName("cup")
                    .setInstance("brewed_coffee")
                    .setMessage("add_milk")
                    .build())
            .setAutority(BuildUAuthority()
                    .setIp("2001::dead:beef")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);
        EXPECT_EQ(zKey, "");
    }
    {
        auto uuri = BuildUUri()
            .setEntity(BuildUEntity()
                    .setName("coffee")
                    .setMajorVersion(0xee)
                    .setId(0xc0ff)
                    .build())
            .setResource(BuildUResource()
                    .setName("cup")
                    .setInstance("brewed_coffee")
                    .setMessage("add_milk")
                    .setID(0x99)
                    .build())
            .setAutority(BuildUAuthority()
                    .setName("2001::dead:beef")
                    .build())
            .build();
        auto zKey = toZenohKeyString(uuri);
        EXPECT_EQ(zKey, "");
    }
}

TEST(TestZenohUri, LongAuthority) {
    auto uuri = BuildUUri()
        .setAutority(BuildUAuthority()
                .setName("2001::dead:beef")
                .build())
        .build();
    auto zKey = toZenohKeyString(uuri);
    EXPECT_EQ(zKey, "");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
