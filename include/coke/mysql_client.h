#ifndef COKE_MYSQL_CLIENT_H
#define COKE_MYSQL_CLIENT_H

#include <string>

#include "coke/network.h"

#include "workflow/MySQLMessage.h"

namespace coke {

using MySQLRequest = protocol::MySQLRequest;
using MySQLResponse = protocol::MySQLResponse;
using MySQLResult = NetworkResult<MySQLResponse>;

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
    using AwaiterType = NetworkAwaiter<ReqType, RespType>;

public:
    explicit MySQLClient(const MySQLClientParams &params);
    virtual ~MySQLClient() = default;

    AwaiterType request(const std::string &query);

protected:
    MySQLClientParams params;
    std::string url;
};

} // namespace coke

#endif // COKE_MYSQL_CLIENT_H
