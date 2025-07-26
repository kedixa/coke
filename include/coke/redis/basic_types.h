/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#ifndef COKE_REDIS_BASIC_TYPES_H
#define COKE_REDIS_BASIC_TYPES_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "coke/net/client_conn_info.h"
#include "coke/utils/str_holder.h"

namespace coke {

struct RedisKeyValue {
    StrHolder key;
    StrHolder value;
};

struct RedisFieldElement {
    StrHolder field;
    StrHolder element;
};

using RedisKeys = StrHolderVec;
using RedisFields = StrHolderVec;
using RedisElements = StrHolderVec;
using RedisKeyValues = std::vector<RedisKeyValue>;
using RedisFieldElements = std::vector<RedisFieldElement>;

// clang-format off
enum : uint16_t {
    REDIS_FLAG_READONLY = 0x01,
};

enum : int16_t {
    REDIS_AUTO_SLOT   = 16384,
    REDIS_ANY_PRIMARY = 16385,
    REDIS_ALL_PRIMARY = 16386,
};

enum : int {
    REDIS_ERR_NO_INFO    = 1,
    REDIS_ERR_NO_COMMAND = 2,
    REDIS_ERR_AUTH       = 3,
    REDIS_ERR_SETNAME    = 4,
    REDIS_ERR_SELECT     = 5,
    REDIS_ERR_READONLY   = 6,
    REDIS_ERR_TRACKING   = 7,
    REDIS_ERR_LIBNAME    = 8,
    REDIS_ERR_LIBVER     = 9,
    REDIS_ERR_NOEVICT    = 10,
    REDIS_ERR_NOTOUCH    = 11,

    REDIS_ERR_INVALID_SLOT     = 30,
    REDIS_ERR_INCOMPLETE_SLOT  = 31,
    REDIS_ERR_GET_SLOT_FAILED  = 32,
    REDIS_ERR_INVALID_REDIRECT = 33,
};
// clang-format on

struct RedisExecuteOption {
    int16_t slot = REDIS_AUTO_SLOT;
    uint16_t flags = 0;
    int32_t block_ms = -1;
};

struct RedisClientInfo {
    bool pipe_handshake;
    bool read_replica;

    int protover;
    int database;
    std::string username;
    std::string password;
    std::string client_name;
    std::string lib_name;
    std::string lib_ver;

    bool no_evict;
    bool no_touch;

    bool enable_tracking;
    bool tracking_bcast;
    bool tracking_optin;
    bool tracking_optout;
    bool tracking_noloop;
    std::string redirect_client_id;
    std::vector<std::string> tracking_prefixes;

    // Attention: Update full_info when RedisClientInfo has new members.
    ClientConnInfo conn_info;
};

} // namespace coke

#endif // COKE_REDIS_BASIC_TYPES_H
