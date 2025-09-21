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

#ifndef COKE_REDIS_CLIENT_H
#define COKE_REDIS_CLIENT_H

#include "coke/redis/client_impl.h"

#include "coke/redis/commands/bitmap.h"
#include "coke/redis/commands/generic.h"
#include "coke/redis/commands/hash.h"
#include "coke/redis/commands/hyperloglog.h"
#include "coke/redis/commands/list.h"
#include "coke/redis/commands/publish.h"
#include "coke/redis/commands/string.h"
#include "coke/redis/commands/transaction.h"

namespace coke {

/**
 * @brief RedisClient is used to communicate with redis server.
 *
 * This client is concurrency-safe, the same client instance can be used across
 * multiple coroutines at the same time. It's generally recommended to create a
 * client once and reuse it for multiple requests, rather than creating a new
 * client for each request.
 *
 * Example:
 *
 * ```cpp
 * coke::RedisClientParams params = {
 *     .host = "127.0.0.1",
 *     .port = "6379",
 *     .password = "your_password",
 * };
 *
 * coke::RedisClient cli(params);
 *
 * coke::RedisResult result = co_await cli.ping();
 * // and then use the result
 * ```
 */
class RedisClient : public RedisBitmapCommands<RedisClient>,
                    public RedisGenericCommands<RedisClient>,
                    public RedisHashCommands<RedisClient>,
                    public RedisHyperloglogCommands<RedisClient>,
                    public RedisListCommands<RedisClient>,
                    public RedisPublishCommands<RedisClient>,
                    public RedisStringCommands<RedisClient>,
                    public RedisClientImpl {
public:
    /**
     * @brief Default constructor. If the default constructor is used, the
     *        `init` function must be called exactly once to initialize the
     *        client before making a request.
     */
    RedisClient() = default;

    /**
     * @brief Constructor for RedisClient.
     * @param params The parameters for the client.
     */
    explicit RedisClient(const RedisClientParams &params)
        : RedisClientImpl(params, false)
    {
    }

    ~RedisClient() = default;
};

/**
 * @brief RedisConnectionClient is used to communicate with redis server.
 *
 * This client is NOT concurrency-safe, the same client can only be used in one
 * coroutine at a time, while different clients have no such restrictions.
 *
 * This client always use the same network connection, if the connection is
 * unexpectedly terminated, the request return with state coke::STATE_SYS_ERROR
 * and error ECONNRESET. The connection will be re-established on the next
 * request.
 *
 * If the frequency of using this client is very low, a large keep_alive_timeout
 * needs to be set to prevent the connection from being frequently closed.
 *
 * @attention If specify the target using params.host, use an IP address to
 * ensure that only one target can be connected. If a domain name is used, the
 * first address will be selected. Upstream name cannot be used as host.
 *
 * @attention After use, the connection needs to be closed through disconnect.
 * Otherwise, the connection will remain open until keep_alive_timeout expires,
 * or until the server close the connection.
 */
class RedisConnectionClient
    : public RedisBitmapCommands<RedisConnectionClient>,
      public RedisGenericCommands<RedisConnectionClient>,
      public RedisHashCommands<RedisConnectionClient>,
      public RedisHyperloglogCommands<RedisClient>,
      public RedisListCommands<RedisConnectionClient>,
      public RedisPublishCommands<RedisClient>,
      public RedisStringCommands<RedisConnectionClient>,
      public RedisTransactionCommand<RedisClient>,
      public RedisClientImpl {
public:
    /**
     * @brief Default constructor. If the default constructor is used, the
     *        `init` function must be called exactly once to initialize the
     *        client before making a request.
     */
    RedisConnectionClient() = default;

    /**
     * @brief Constructor for RedisClient.
     * @param params The parameters for the client.
     */
    explicit RedisConnectionClient(const RedisClientParams &params)
        : RedisClientImpl(params, true)
    {
    }

    ~RedisConnectionClient() = default;

    /**
     * @brief Close the associated network connection.
     */
    Task<RedisResult> disconnect()
    {
        close_connection = true;
        return execute_command({"PING"});
    }
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_H
