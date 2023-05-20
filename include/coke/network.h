#ifndef COKE_NETWORK_H
#define COKE_NETWORK_H

#include <optional>
#include <utility>

#include "coke/coke.h"
#include "workflow/WFTask.h"

namespace coke {

template<typename RESP>
struct NetworkResult {
    int state;
    int error;
    long long seqid;
    RESP resp;
    // TODO addr, timeout reason
};

template<typename REQ, typename RESP>
class NetworkAwaiter : public AwaiterBase {
    using req_t = REQ;
    using resp_t = RESP;
    using task_t = WFNetworkTask<req_t, resp_t>;

public:
    explicit NetworkAwaiter(task_t *task, bool reply = false) {
        task->set_callback([this] (task_t *t) {
            using result_t = NetworkResult<resp_t>;
            ret.emplace(result_t{t->get_state(), t->get_error(),
                        t->get_task_seq(), std::move(*t->get_resp())});
            this->done();
        });

        set_task(task, reply);
    }

    NetworkResult<resp_t> await_resume() {
        assert(ret.has_value());
        return std::move(ret.value());
    }

private:
    std::optional<NetworkResult<resp_t>> ret;
};


struct ReplyResult {
    int state;
    int error;
};


class ReplyAwaiter : public AwaiterBase {
public:
    template<typename REQ, typename RESP>
    explicit ReplyAwaiter(WFNetworkTask<REQ, RESP> *task) {
        task->set_callback([this] (WFNetworkTask<REQ, RESP> *t) {
            ret = {t->get_state(), t->get_error()};
            this->done();
        });

        set_task(task, true);
    }

    ReplyResult await_resume() {
        return ret;
    }

private:
    ReplyResult ret;
};


template<typename REQ, typename RESP>
class ServerContext {
    using req_t = REQ;
    using resp_t = RESP;
    using task_t = WFNetworkTask<req_t, resp_t>;
    using awaiter_t = ReplyAwaiter;

public:
    ServerContext(task_t *task) : replied(false), task(task)
    { }

    ServerContext(ServerContext &&c) {
        replied = c.replied;
        task = c.task;

        // c cannot be used any more
        c.replied = true;
        c.task = nullptr;
    }

    ServerContext &operator=(ServerContext &&c) {
        if (this != &c) {
            std::swap(task, c.task);
            std::swap(replied, c.replied);
        }
        return *this;
    }

    req_t &get_req() & { return *(task->get_req()); }
    resp_t &get_resp() & { return *(task->get_resp()); }
    long long get_seqid() { return task->get_seq(); }

    ReplyAwaiter reply() {
        // Each ServerContext must reply once
        assert(!replied);
        replied = true;

        return ReplyAwaiter(task);
    }

private:
    bool replied;
    task_t *task;
};

} // namespace coke

#endif // COKE_NETWORK_H
