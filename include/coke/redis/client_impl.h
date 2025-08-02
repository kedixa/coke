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

#ifndef COKE_REDIS_CLIENT_IMPL_H
#define COKE_REDIS_CLIENT_IMPL_H

#include <memory>
#include <string>

#include <openssl/ossl_typ.h>
#include <sys/socket.h>

#include "coke/redis/basic_types.h"
#include "coke/redis/value.h"
#include "coke/task.h"

namespace coke {

struct RedisClientParams {
    int retry_max{0};
    int send_timeout{-1};
    int receive_timeout{-1};
    int keep_alive_timeout{60 * 1000};

    // Used when blocking commands (like blpop) with timeout 0, infinite block
    // is not recommended by RedisClient.
    int default_watch_timeout{10 * 1000};

    // Due to network latency, the watch timeout is added extra milliseconds
    // when default watch timeout is not used.
    int watch_extra_timeout{1000};

    std::size_t response_size_limit{64 * 1024 * 1024};

    bool use_ssl{false};
    std::shared_ptr<SSL_CTX> ssl_ctx{nullptr};

    std::string host;
    std::string port{"6379"};

    // If host is empty, the addr will be used.
    sockaddr_storage addr_storage{};
    socklen_t addr_len{0};

    // Whether use pipeline request when handshake(auth, select, etc).
    bool pipe_handshake{true};

    int protover{2};
    int database{0};
    std::string username;
    std::string password;
    std::string client_name;
    std::string lib_name;
    std::string lib_ver;
    bool no_evict{false};
    bool no_touch{false};
};

class RedisClientImpl {
public:
    /**
     * @brief Default constructor. If the default constructor is used, the
     *        `init` function must be called exactly once to initialize the
     *        client before making a request.
     */
    RedisClientImpl() = default;

    /**
     * @brief Constructor for RedisClientImpl.
     * @param params The parameters for the client.
     * @param unique_conn See RedisConnectionClient, to ensure requests are
     *        sent on the same connection.
     *
     * @attention Use coke::RedisClient or coke::RedisConnectionClient instead
     *            of this constructor directly.
     */
    explicit RedisClientImpl(const RedisClientParams &params,
                             bool unique_conn = false)
        : params(params)
    {
        init_client(unique_conn);
    }

    /**
     * @brief RedisClientImpl is neither copyable nor movable.
     */
    RedisClientImpl(const RedisClientImpl &) = delete;
    RedisClientImpl(RedisClientImpl &&) = delete;

    RedisClientImpl &operator=(const RedisClientImpl &) = delete;
    RedisClientImpl &operator=(RedisClientImpl &&) = delete;

    ~RedisClientImpl() = default;

    /**
     * @brief Initialize the client, used after default constructor.
     */
    void init(const RedisClientParams &params, bool unique_conn = false)
    {
        this->params = params;
        return init_client(unique_conn);
    }

    /**
     * @brief Get the params used to initialize this client.
     */
    RedisClientParams get_params() const { return params; }

    /**
     * @brief Execute a Redis command.
     * @param command The command to execute.
     *
     * ```cpp
     * coke::StrHolderVec command = {"SET", "key", "value"};
     * auto result = co_await client.execute_command(std::move(command));
     * ```
     */
    Task<RedisResult> execute_command(StrHolderVec command,
                                      RedisExecuteOption opt = {})
    {
        return _execute(std::move(command), opt);
    }

    /**
     * @attention This function is for internal use, do not call it directly.
     */
    Task<RedisResult> _execute(StrHolderVec command, RedisExecuteOption opt);

protected:
    void init_client(bool unique_conn);

protected:
    bool close_connection{false};
    RedisClientParams params;
    RedisClientInfo cli_info;
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_IMPL_H
