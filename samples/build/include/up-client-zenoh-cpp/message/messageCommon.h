/*
 * Copyright (c) 2023 General Motors GTO LLC
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
 * SPDX-FileCopyrightText: 2023 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MESSAGE_COMMON_H_
#define _MESSAGE_COMMON_H_

enum class Tag
{
    UURI = 0,
    ID = 1, 
    TYPE = 2,
    PRIORITY = 3,
    TTL = 4,
    TOKEN = 5,
    HINT = 6,
    SINK = 7,
    PLEVEL = 8,
    COMMSTATUS = 9,
    REQID = 10,
    PAYLOAD = 11, 

    UNDEFINED = 12
};

#endif /* _MESSAGE_COMMON_H_ */