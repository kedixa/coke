#ifndef COKE_REDIS_CLIENT_H
#define COKE_REDIS_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/basic_client.h"
#include "workflow/RedisMessage.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisResult = NetworkResult<RedisResponse>;

struct RedisClientParams : public ClientParams {
    bool use_ssl{false};
    int port{6379};
    int db{0};
    std::string host;
    std::string password;
};

class RedisClient final : public BasicClient<RedisRequest, RedisResponse> {
    using Base = BasicClient<RedisRequest, RedisResponse>;

public:
    using Base::awaiter_t;
    using Base::task_t;

public:
    RedisClient(const RedisClientParams &p);

    awaiter_t request(const std::string &command, std::vector<std::string> params);

protected:
    task_t *create_task(const std::string &, req_t *) override;

private:
    std::string url;
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_H
