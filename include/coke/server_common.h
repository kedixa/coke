#ifndef COKE_SERVER_COMMON_H
#define COKE_SERVER_COMMON_H

#include "coke/network.h"
#include "workflow/WFServer.h"

namespace coke {

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

using ServerParams = WFServerParams;

template<typename REQ, typename RESP>
class BasicServer : public WFServer<REQ, RESP> {
    using BaseType = WFServer<REQ, RESP>;

public:
    using ReqType = REQ;
    using RespType = RESP;
    using TaskType = WFNetworkTask<ReqType, RespType>;
    using ServerContextType = ServerContext<TaskType>;
    using ReplyResultType = NetworkReplyResult;
    using ProcessorType = std::function<Task<>(ServerContextType)>;

private:
    static auto get_process(BasicServer *server) {
        return [server](TaskType *task) { return server->do_proc(task); };
    }

public:
    BasicServer(const ServerParams &params, ProcessorType co_proc)
        : WFServer<REQ, RESP>(&params, get_process(this)),
          co_proc(std::move(co_proc))
    { }

protected:
    virtual void do_proc(TaskType *task) {
        Task<> t = co_proc(ServerContextType(task));
        t.start_on_series(series_of(task));
    }

private:
    ProcessorType co_proc;
};

} // namespace coke

#endif // COKE_SERVER_COMMON_H
