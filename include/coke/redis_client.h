#ifndef COKE_REDIS_CLIENT_H
#define COKE_REDIS_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/network.h"

#include "workflow/RedisMessage.h"
#include "workflow/URIParser.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisValue = protocol::RedisValue;
using RedisAwaiter = NetworkAwaiter<RedisRequest, RedisResponse>;
using RedisResult = RedisAwaiter::ResultType;

struct RedisClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;

    bool use_ssl            = false;
    int port                = 6379;
    int db                  = 0;
    std::string host;
    std::string username;
    std::string password;
};

class RedisClient {
public:
    using ReqType = RedisRequest;
    using RespType = RedisResponse;
    using AwaiterType = RedisAwaiter;

public:
    explicit RedisClient(const RedisClientParams &params);
    virtual ~RedisClient() = default;

    [[nodiscard]]
    AwaiterType request(const std::string &command,
                        const std::vector<std::string> &params) noexcept;

protected:
    virtual AwaiterType create_task(ReqType *) noexcept;

protected:
    RedisClientParams params;
    std::string url;
    ParsedURI uri;
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_H
