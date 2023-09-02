#ifndef COKE_MYSQL_CLIENT_H
#define COKE_MYSQL_CLIENT_H

#include <string>

#include "coke/network.h"

#include "workflow/MySQLMessage.h"
#include "workflow/URIParser.h"

namespace coke {

using MySQLRequest = protocol::MySQLRequest;
using MySQLResponse = protocol::MySQLResponse;
using MySQLResult = NetworkResult<MySQLResponse>;
using MySQLAwaiter = NetworkAwaiter<MySQLRequest, MySQLResponse>;

struct MySQLClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;

    bool use_ssl            = false;
    int port                = 3306;
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
    explicit MySQLClient(const MySQLClientParams &params) : MySQLClient(params, false, 0) { }
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
    MySQLClient(const MySQLClientParams &params, bool unique_conn, std::size_t conn_id);

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
 * will be determined by the **globally unique transaction_id**.
 *
 * This feature can be used to implement database transactions. When a request
 * fails, the current connection will be closed, and the connection will be
 * re-established for the next request.
 *
*/
class MySQLConnection : public MySQLClient {
public:
    /**
     * Each call to this function will return an auto-incremented id,
     * which can be used to create a MySQLConnection.
    */
    static std::size_t get_unique_id();

public:
    explicit MySQLConnection(const MySQLClientParams &params, std::size_t conn_id)
        : MySQLClient(params, true, conn_id)
    { }

    std::size_t get_conn_id() const { return conn_id; }

    /**
     * Disconnect this Connection.
     *
     * When the object is no longer used, it is best to close the connection.
    */
    AwaiterType disconnect();
};

} // namespace coke

#endif // COKE_MYSQL_CLIENT_H
