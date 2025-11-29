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

#ifndef COKE_MYSQL_CLIENT_H
#define COKE_MYSQL_CLIENT_H

#include <string>

#include "coke/net/network.h"

#include "workflow/MySQLMessage.h"
#include "workflow/URIParser.h"

namespace coke {

using MySQLRequest = protocol::MySQLRequest;
using MySQLResponse = protocol::MySQLResponse;
using MySQLAwaiter = NetworkAwaiter<MySQLRequest, MySQLResponse>;
using MySQLResult = MySQLAwaiter::ResultType;

struct MySQLClientParams {
    int retry_max = 0;
    int send_timeout = -1;
    int receive_timeout = -1;
    int keep_alive_timeout = 60 * 1000;

    bool use_ssl = false;
    int port = 3306;
    std::string host;
    std::string username;
    std::string password;
    std::string dbname;
    std::string character_set = "utf8";
    std::string character_set_results;
};

class MySQLClient {
public:
    using ReqType = MySQLRequest;
    using RespType = MySQLResponse;
    using AwaiterType = MySQLAwaiter;

public:
    explicit MySQLClient(const MySQLClientParams &params)
        : MySQLClient(params, false, 0)
    {
    }

    virtual ~MySQLClient() = default;

    /**
     * Return the `params` to create this instance, except retry will be
     * force reset to zero when this is MySQLConnection.
     */
    MySQLClientParams get_params() const { return params; }

    /**
     * Make a MySQL request, for example
     *
     * MySQLClient cli(params);
     * MySQLResult res = co_await cli.request("show tables;");
     * // check res.status and view response
     */
    AwaiterType request(const std::string &query);

protected:
    MySQLClient(const MySQLClientParams &params, bool unique_conn,
                std::size_t conn_id);

protected:
    bool unique_conn;
    std::size_t conn_id;
    MySQLClientParams params;

    std::string url;
    ParsedURI uri;
};

/**
 * MySQLConnection is a kind of MySQLClient, but all requests need to be sent
 * serially from the same connection, the uniqueness of the connection
 * will be determined by the **globally unique connection id**.
 *
 * This feature can be used to implement database transactions. When a request
 * fails, the current connection will be closed, and the connection will be
 * re-established for the next request.
 *
 */
class MySQLConnection : public MySQLClient {
    static std::size_t acquire_conn_id();
    static void release_conn_id(std::size_t conn_id);

public:
    explicit MySQLConnection(const MySQLClientParams &params)
        : MySQLClient(params, true, acquire_conn_id())
    {
    }

    virtual ~MySQLConnection() { release_conn_id(conn_id); }

    /**
     * @brief Disconnect this Connection.
     *
     * When the object is no longer used, the connection MUST be disconnected.
     */
    AwaiterType disconnect();
};

} // namespace coke

#endif // COKE_MYSQL_CLIENT_H
