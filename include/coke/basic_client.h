#ifndef COKE_BASIC_CLIENT_H
#define COKE_BASIC_CLIENT_H

#include "coke/network.h"
#include "workflow/WFTask.h"

namespace coke {

struct ClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;
};


template<typename REQ, typename RESP>
class BasicClient {
public:
    using req_t = REQ;
    using resp_t = RESP;
    using task_t = WFNetworkTask<req_t, resp_t>;
    using awaiter_t = NetworkAwaiter<req_t, resp_t>;

public:
    explicit BasicClient(const ClientParams &p = ClientParams())
        : params(p) { }

    BasicClient(const BasicClient &) = default;
    virtual ~BasicClient() = default;

    virtual awaiter_t request(const std::string &url);
    virtual awaiter_t request(const std::string &url, req_t &&req);

protected:
    virtual task_t *create_task(const std::string &url, req_t *req) = 0;
    virtual awaiter_t get_awaiter(task_t *task);

protected:
    ClientParams params;
};


template<typename REQ, typename RESP>
typename BasicClient<REQ, RESP>::awaiter_t
BasicClient<REQ, RESP>::request(const std::string &url) {
    return get_awaiter(create_task(url, nullptr));
}

template<typename REQ, typename RESP>
typename BasicClient<REQ, RESP>::awaiter_t
BasicClient<REQ, RESP>::request(const std::string &url, req_t &&req) {
    return get_awaiter(create_task(url, &req));
}

template<typename REQ, typename RESP>
typename BasicClient<REQ, RESP>::awaiter_t
BasicClient<REQ, RESP>::get_awaiter(task_t *task) {
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);
    return awaiter_t(task);
}

} // namespace coke

#endif // COKE_BASIC_CLIENT_H
