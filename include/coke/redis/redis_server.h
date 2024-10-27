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

#ifndef COKE_REDIS_SERVER_H
#define COKE_REDIS_SERVER_H

#include "coke/net/basic_server.h"

#include "workflow/WFRedisServer.h"
#include "workflow/RedisMessage.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisValue = protocol::RedisValue;
using RedisServerContext = ServerContext<RedisRequest, RedisResponse>;
using RedisReplyResult = NetworkReplyResult;

struct RedisServerParams : public ServerParams {
    RedisServerParams() : ServerParams(REDIS_SERVER_PARAMS_DEFAULT) { }
    ~RedisServerParams() = default;
};

class RedisServer : public BasicServer<RedisRequest, RedisResponse> {
public:
    RedisServer(const RedisServerParams &params, ProcessorType co_proc)
        : BasicServer<RedisRequest, RedisResponse>(params, std::move(co_proc))
    { }

    RedisServer(ProcessorType co_proc)
        : BasicServer<RedisRequest, RedisResponse>(RedisServerParams(),
                                                   std::move(co_proc))
    { }
};

} // namespace coke

#endif // COKE_REDIS_SERVER_H
