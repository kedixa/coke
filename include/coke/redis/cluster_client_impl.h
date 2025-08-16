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

#ifndef COKE_REDIS_CLUSTER_CLIENT_IMPL_H
#define COKE_REDIS_CLUSTER_CLIENT_IMPL_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include <openssl/ossl_typ.h>
#include <sys/socket.h>

#include "coke/mutex.h"
#include "coke/redis/basic_types.h"
#include "coke/redis/value.h"
#include "coke/task.h"

namespace coke {

struct RedisClusterClientParams {
    int retry_max{0};
    int send_timeout{-1};
    int receive_timeout{-1};
    int keep_alive_timeout{60 * 1000};

    // Used when blocking commands (like blpop) with timeout 0, infinite block
    // is not recommended by RedisClusterClient.
    int default_watch_timeout{10 * 1000};

    // Due to network latency, the watch timeout is added extra milliseconds
    // when default watch timeout is not used.
    int watch_extra_timeout{1000};

    std::size_t response_size_limit{64 * 1024 * 1024};

    bool use_ssl{false};
    std::shared_ptr<SSL_CTX> ssl_ctx{nullptr};

    std::string host;
    std::string port{"6379"};

    // Whether use pipeline request when handshake(auth, select, etc).
    bool pipe_handshake{true};

    // Whether to read from replica nodes.
    bool read_replica{false};

    int protover{2};
    std::string username;
    std::string password;
    std::string client_name;
    std::string lib_name;
    std::string lib_ver;
    bool no_evict{false};
    bool no_touch{false};
};

struct RedisSlotNode {
    std::string host;
    std::string port;
    std::string node_id;

    sockaddr_storage addr_storage{};
    socklen_t addr_len{0};
};

using RedisSlotNodes = std::vector<RedisSlotNode>;

struct RedisSlotsTable {
    int state;
    int error;

    bool complete;
    uint64_t version;
    std::atomic<bool> outdated;
    std::vector<int> slot_index;
    std::vector<RedisSlotNodes> nodes_vec;

    std::vector<RedisSlotNode> all_primaries;
    std::vector<RedisSlotNode> all_nodes;
};

using RedisSlotsTablePtr = std::shared_ptr<RedisSlotsTable>;

class RedisClusterClientImpl {
public:
    using RedisClusterClientTag = void;

    /**
     * @brief Default constructor. If the default constructor is used, the
     *        `init` function must be called exactly once to initialize the
     *        client before making a request.
     */
    RedisClusterClientImpl() = default;

    /**
     * @brief Constructor for RedisClusterClientImpl.
     * @param params The parameters for the client.
     *
     * @attention Use coke::RedisClusterClient instead of this constructor
     *            directly.
     */
    explicit RedisClusterClientImpl(const RedisClusterClientParams &params)
        : params(params)
    {
        init_client();
    }

    /**
     * @brief RedisClusterClientImpl is neither copyable nor movable.
     */
    RedisClusterClientImpl(const RedisClusterClientImpl &) = delete;
    RedisClusterClientImpl(RedisClusterClientImpl &&) = delete;

    RedisClusterClientImpl &operator=(const RedisClusterClientImpl &) = delete;
    RedisClusterClientImpl &operator=(RedisClusterClientImpl &&) = delete;

    ~RedisClusterClientImpl() = default;

    /**
     * @brief Initialize the client, used after default constructor.
     */
    void init(const RedisClusterClientParams &params)
    {
        this->params = params;
        return init_client();
    }

    /**
     * @brief Get the params used to initialize this client.
     */
    RedisClusterClientParams get_params() const { return params; }

    /**
     * @brief Execute a Redis command.
     * @param command The command to execute.
     * @param opt The options for executing the command. See RedisExecuteOption.
     */
    Task<RedisResult> execute_command(StrHolderVec command,
                                      RedisExecuteOption opt)
    {
        return _execute(std::move(command), opt);
    }

    /**
     * @attention This function is for internal use, do not call it directly.
     */
    Task<RedisResult> _execute(StrHolderVec command, RedisExecuteOption opt);

protected:
    void init_client();

    Task<RedisResult> execute_impl(RedisSlotsTablePtr &table,
                                   const RedisSlotNode &node, int retry_max,
                                   const StrHolderVec &command,
                                   const RedisExecuteOption &opt);

    RedisSlotsTablePtr get_slots_table()
    {
        std::shared_lock lk(table_mtx);
        return slots_table;
    }

    Task<RedisSlotsTablePtr> update_slots_table(uint64_t old_version);

    Task<RedisSlotsTablePtr> update_table_impl(const RedisSlotNode &node,
                                               int retry_max);

    RedisSlotsTablePtr parse_slots(const RedisValue &value);

protected:
    RedisClusterClientParams params;
    RedisClientInfo cli_info;
    RedisSlotNode init_node;
    std::atomic<std::size_t> choice_cnt{0};

    std::shared_mutex table_mtx;
    Mutex co_table_mtx;
    RedisSlotsTablePtr slots_table;
};

} // namespace coke

#endif // COKE_REDIS_CLUSTER_CLIENT_IMPL_H
