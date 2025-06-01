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

#ifndef COKE_TLV_CLIENT_H
#define COKE_TLV_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "coke/task.h"
#include "coke/tlv/basic_types.h"
#include "coke/utils/str_holder.h"
#include "workflow/EndpointParams.h"

namespace coke {

struct TlvClientParams {
    TransportType transport_type{TT_TCP};

    int retry_max{0};
    int send_timeout{-1};
    int receive_timeout{-1};
    int keep_alive_timeout{60 * 1000};
    int watch_timeout{0};

    std::string host;
    std::string port;

    bool enable_auth{false};
    int32_t auth_type{0};
    int32_t auth_success_type{0};
    std::string auth_value;

    std::size_t response_size_limit{(std::size_t)-1};
};

/**
 * @brief TlvClient is used to communicate with TlvServer.
 *
 * This client is concurrency-safe, the same client instance can be used across
 * multiple coroutines at the same time. It's generally recommended to create a
 * client once and reuse it for multiple requests, rather than creating a new
 * client for each request.
 *
 * Example:
 *
 * ```cpp
 * coke::TlvClientParams params = {
 *     .host = "127.0.0.1",
 *     .port = "5678",
 * };
 *
 * coke::TlvClient cli(params);
 *
 * coke::TlvResult result = co_await cli.request(type, value);
 * // and then use the result
 * ```
 */
class TlvClient {
public:
    explicit TlvClient(const TlvClientParams &params) : TlvClient(params, false)
    {
    }

    /**
     * @brief The TlvClient is neither copyable nore movable.
     */
    TlvClient(const TlvClient &) = delete;
    TlvClient(TlvClient &&)      = delete;

    ~TlvClient() = default;

    /**
     * @brief Request with type and value, and wait for TlvResult.
     */
    Task<TlvResult> request(int32_t type, StrHolder value);

protected:
    TlvClient(const TlvClientParams &params, bool unique_conn);

protected:
    bool close_connection;
    TlvClientParams params;
    TlvClientInfo cli_info;
};

/**
 * @brief TlvConnectionClient is used to communicate with TlvServer.
 *
 * This client always use the same network connection, if the connection is
 * unexpectedly terminated, the request return with state coke::STATE_SYS_ERROR
 * and error ECONNRESET. The connection will be re-established on the next
 * request.
 *
 * If the frequency of using this client is very low, a large keep_alive_timeout
 * needs to be set to prevent the connection from being frequently closed.
 *
 * @attention After use, the connection needs to be closed through disconnect.
 * Otherwise, the connection will remain open until keep_alive_timeout expires,
 * or until the server close the connection.
 */
class TlvConnectionClient : public TlvClient {
public:
    explicit TlvConnectionClient(const TlvClientParams &params)
        : TlvClient(params, true)
    {
    }

    ~TlvConnectionClient() = default;

    /**
     * @brief Close the associated network connection.
     */
    Task<TlvResult> disconnect()
    {
        close_connection = true;
        return request(0, "");
    }
};

} // namespace coke

#endif // COKE_TLV_CLIENT_H
