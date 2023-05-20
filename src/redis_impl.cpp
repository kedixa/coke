#include "coke/redis_client.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

RedisClient::RedisClient(const RedisClientParams &p) : BasicClient(p) {
    url.assign(p.use_ssl ? "rediss://" : "redis://");

    if (!p.password.empty())
        url.append(":").append(p.password).append("@");

    url.append(p.host).append(":").append(std::to_string(p.port));
    url.append("/").append(std::to_string(p.db));
}

RedisClient::awaiter_t
RedisClient::request(const std::string &command, std::vector<std::string> params) {
    RedisRequest req;
    req.set_request(command, params);

    return Base::request("", std::move(req));
}

RedisClient::task_t *
RedisClient::create_task(const std::string &, req_t *req) {
    WFRedisTask *task;
    
    task = WFTaskFactory::create_redis_task(url, params.retry_max, nullptr);
    if (req)
        *(task->get_req()) = std::move(*req);

    return task;
}

} // namespace coke
