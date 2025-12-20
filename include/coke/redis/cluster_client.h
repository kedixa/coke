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

#ifndef COKE_REDIS_CLUSTER_CLIENT_H
#define COKE_REDIS_CLUSTER_CLIENT_H

#include "coke/redis/cluster_client_impl.h"

#include "coke/redis/commands/bitmap.h"
#include "coke/redis/commands/generic.h"
#include "coke/redis/commands/hash.h"
#include "coke/redis/commands/hyperloglog.h"
#include "coke/redis/commands/list.h"
#include "coke/redis/commands/publish.h"
#include "coke/redis/commands/set.h"
#include "coke/redis/commands/string.h"

namespace coke {

/**
 * @brief RedisClusterClient is used to communicate with redis cluster.
 *
 * This client is concurrency-safe, the same client instance can be used across
 * multiple coroutines at the same time. It's generally recommended to create a
 * client once and reuse it for multiple requests, rather than creating a new
 * client for each request.
 *
 * Example:
 *
 * ```cpp
 * coke::RedisClusterClientParams params = {
 *     .host = "127.0.0.1",
 *     .port = "6379",
 *     .password = "your_password",
 * };
 *
 * coke::RedisClusterClient cli(params);
 *
 * coke::RedisResult result = co_await cli.ping();
 * // and then use the result
 * ```
 */
class RedisClusterClient : public RedisBitmapCommands<RedisClusterClient>,
                           public RedisGenericCommands<RedisClusterClient>,
                           public RedisHashCommands<RedisClusterClient>,
                           public RedisHyperloglogCommands<RedisClusterClient>,
                           public RedisListCommands<RedisClusterClient>,
                           public RedisPublishCommands<RedisClusterClient>,
                           public RedisSetCommands<RedisClusterClient>,
                           public RedisStringCommands<RedisClusterClient>,
                           public RedisClusterClientImpl {
public:
    /**
     * @brief Default constructor. If the default constructor is used, the
     *        `init` function must be called exactly once to initialize the
     *        client before making a request.
     */
    RedisClusterClient() = default;

    /**
     * @brief Constructor for RedisClusterClient.
     * @param params The parameters for the client.
     */
    explicit RedisClusterClient(const RedisClusterClientParams &params)
        : RedisClusterClientImpl(params)
    {
    }

    ~RedisClusterClient() = default;
};

} // namespace coke

#endif // COKE_REDIS_CLUSTER_CLIENT_H
