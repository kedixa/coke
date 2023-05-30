#ifndef COKE_REDIS_CLIENT_H
#define COKE_REDIS_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/network.h"

#include "workflow/RedisMessage.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisResult = NetworkResult<RedisResponse>;

struct RedisClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;

    bool use_ssl            = false;
    int port                = 6379;
    int db                  = 0;
    std::string host;
    std::string password;
};

class RedisClient {
public:
    using ReqType = RedisRequest;
    using RespType = RedisResponse;
    using AwaiterType = NetworkAwaiter<ReqType, RespType>;

public:
    explicit
    RedisClient(const RedisClientParams &params = RedisClientParams());
    virtual ~RedisClient() = default;

    AwaiterType request(std::string command, std::vector<std::string> params) noexcept;

    // TODO: Just an example, I'm thinking about it
    // without consider request fail, return Task<std::string> (which will produce "PONG")
    // is much better
    AwaiterType ping() noexcept {
        return request("PING", {});
    }

protected:
    virtual AwaiterType create_task(ReqType *);

protected:
    RedisClientParams params;
    std::string url;
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_H
