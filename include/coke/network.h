#ifndef COKE_NETWORK_H
#define COKE_NETWORK_H

#include <optional>
#include <utility>
#include <cassert>

#include "coke/detail/basic_concept.h"
#include "coke/detail/awaiter_base.h"

namespace coke {

template<typename RESP>
struct NetworkResult {
    using RespType = RESP;

    int state;
    int error;
    int timeout_reason;
    long long seqid;
    RespType resp;
};

template<typename REQ, typename RESP>
class NetworkAwaiter : public BasicAwaiter<NetworkResult<RESP>> {
public:
    using ReqType = REQ;
    using RespType = RESP;
    using ResultType = NetworkResult<RespType>;

public:
    template<NetworkTaskType NT>
        requires (std::same_as<typename NetworkTaskTrait<NT>::ReqType, ReqType> &&
                std::same_as<typename NetworkTaskTrait<NT>::RespType, RespType>)
    explicit NetworkAwaiter(NT *task, bool reply = false) {
        task->set_callback([info = this->get_info()] (NT *t) {
            using Atype = NetworkAwaiter<ReqType, RespType>;
            auto *awaiter = info->template get_awaiter<Atype>();
            awaiter->emplace_result(
                ResultType {
                    t->get_state(), t->get_error(), t->get_timeout_reason(),
                    t->get_task_seq(), std::move(*t->get_resp())
                }
            );

            awaiter->done();
        });

        this->set_task(task, reply);
    }
};


struct NetworkReplyResult {
    int state;
    int error;
};

class NetworkReplyAwaiter : public BasicAwaiter<NetworkReplyResult> {
public:
    template<NetworkTaskType NT>
    explicit NetworkReplyAwaiter(NT *task) {
        task->set_callback([info = this->get_info()] (NT *t) {
            auto *awaiter = info->get_awaiter<NetworkReplyAwaiter>();
            awaiter->emplace_result(NetworkReplyResult{
                t->get_state(), t->get_error()
            });
            awaiter->done();
        });

        this->set_task(task, true);
    }
};


template<NetworkTaskType NT>
class ServerContext {
    using ReqType = typename NetworkTaskTrait<NT>::ReqType;
    using RespType = typename NetworkTaskTrait<NT>::RespType;
    using AwaiterType = NetworkReplyAwaiter;

public:
    ServerContext(NT *task) : replied(false), task(task)
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

    ReqType &get_req() & { return *(task->get_req()); }
    RespType &get_resp() & { return *(task->get_resp()); }
    long long get_seqid() { return task->get_seq(); }

    AwaiterType reply() {
        // Each ServerContext must reply once
        assert(!replied);
        replied = true;

        return AwaiterType(task);
    }

private:
    bool replied;
    NT *task;
};

} // namespace coke

#endif // COKE_NETWORK_H
