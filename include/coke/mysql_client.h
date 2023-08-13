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

    std::size_t transaction_id = 0;
};

class MySQLClient {
public:
    using ReqType = MySQLRequest;
    using RespType = MySQLResponse;
    using AwaiterType = MySQLAwaiter;

public:
    explicit MySQLClient(const MySQLClientParams &params);
    virtual ~MySQLClient() = default;

    /**
     * Make a MySQL request, for example
     *
     * MySQLClient cli(params);
     * MySQLResult res = co_await cli.request("show tables;");
     * // check res.status and view response
    */
    AwaiterType request(const std::string &query);

    /**
     * Disconnect Connection with transaction_id.
     * This function can only be called when transaction_id is not zero.
    */
    AwaiterType disconnect();

protected:
    MySQLClientParams params;
    std::string url;
    ParsedURI uri;
};

} // namespace coke

#endif // COKE_MYSQL_CLIENT_H
