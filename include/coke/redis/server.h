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

#ifndef COKE_REDIS_SERVER_H
#define COKE_REDIS_SERVER_H

#include "coke/net/basic_server.h"
#include "coke/redis/message.h"

namespace coke {

struct RedisServerParams {
    TransportType transport_type{TT_TCP};
    std::size_t max_connections{2000};
    int peer_response_timeout{10 * 1000};
    int receive_timeout{-1};
    int keep_alive_timeout{300 * 1000};
    std::size_t request_size_limit{SIZE_MAX};
    int ssl_accept_timeout{5 * 1000};

    operator ServerParams() const noexcept
    {
        return detail::to_server_params(*this);
    }
};

class RedisServer : public BasicServer<RedisRequest, RedisResponse> {
    using Base = BasicServer<RedisRequest, RedisResponse>;

public:
    RedisServer(const RedisServerParams &params, ProcessorType co_proc)
        : Base(params, std::move(co_proc))
    {
    }

    RedisServer(ProcessorType co_proc)
        : Base(RedisServerParams{}, std::move(co_proc))
    {
    }
};

using RedisServerContext = RedisServer::ServerContextType;
using RedisProcessorType = RedisServer::ProcessorType;

} // namespace coke

#endif // COKE_REDIS_SERVER_H
