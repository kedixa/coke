/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_REDIS_CLIENT_H
#define COKE_REDIS_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/network.h"

#include "workflow/RedisMessage.h"
#include "workflow/URIParser.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisValue = protocol::RedisValue;
using RedisAwaiter = NetworkAwaiter<RedisRequest, RedisResponse>;
using RedisResult = RedisAwaiter::ResultType;

struct RedisClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;

    bool use_ssl            = false;
    int port                = 6379;
    int db                  = 0;
    std::string host;
    std::string username;
    std::string password;
};

class RedisClient {
public:
    using ReqType = RedisRequest;
    using RespType = RedisResponse;
    using AwaiterType = RedisAwaiter;

public:
    explicit RedisClient(const RedisClientParams &params);
    virtual ~RedisClient() = default;

    AwaiterType request(const std::string &command,
                        const std::vector<std::string> &params) noexcept;

protected:
    virtual AwaiterType create_task(ReqType *) noexcept;

protected:
    RedisClientParams params;
    std::string url;
    ParsedURI uri;
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_H
