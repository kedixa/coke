#ifndef COKE_REDIS_SERVER_H
#define COKE_REDIS_SERVER_H

#include "coke/detail/task.h"
#include "coke/basic_server.h"

#include "workflow/WFRedisServer.h"
#include "workflow/RedisMessage.h"

namespace coke {

using RedisRequest = protocol::RedisRequest;
using RedisResponse = protocol::RedisResponse;
using RedisValue = protocol::RedisValue;
using RedisServerContext = ServerContext<RedisRequest, RedisResponse>;
using RedisReplyResult = NetworkReplyResult;

struct RedisServerParams : public ServerParams {
    RedisServerParams() : ServerParams(REDIS_SERVER_PARAMS_DEFAULT) { }
    ~RedisServerParams() = default;
};

class RedisServer : public BasicServer<RedisRequest, RedisResponse> {
public:
    RedisServer(const RedisServerParams &params, ProcessorType co_proc)
        : BasicServer<RedisRequest, RedisResponse>(params, std::move(co_proc))
    { }

    RedisServer(ProcessorType co_proc)
        : BasicServer<RedisRequest, RedisResponse>(RedisServerParams(), std::move(co_proc))
    { }
};

} // namespace coke

#endif // COKE_REDIS_SERVER_H
