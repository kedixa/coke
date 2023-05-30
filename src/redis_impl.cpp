#include "coke/redis_client.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

RedisClient::RedisClient(const RedisClientParams &params)
    : params(params)
{
    url.assign(params.use_ssl ? "rediss://" : "redis://");

    if (!params.password.empty())
        url.append(":").append(params.password).append("@");

    url.append(params.host).append(":")
       .append(std::to_string(params.port))
       .append("/").append(std::to_string(params.db));
}

RedisClient::AwaiterType
RedisClient::request(std::string command, std::vector<std::string> params) {
    RedisRequest req;
    req.set_request(command, params);

    return create_task(&req);
}

RedisClient::AwaiterType
RedisClient::create_task(ReqType *req) {
    WFRedisTask *task;
    
    task = WFTaskFactory::create_redis_task(url, params.retry_max, nullptr);
    if (req)
        *(task->get_req()) = std::move(*req);

    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);

    return AwaiterType(task);
}

} // namespace coke
