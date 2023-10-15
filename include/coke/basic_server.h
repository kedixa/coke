#ifndef COKE_BASIC_SERVER_H
#define COKE_BASIC_SERVER_H

#include "coke/network.h"
#include "workflow/WFServer.h"

namespace coke {

struct NetworkReplyResult {
    int state;
    int error;
};

class NetworkReplyAwaiter : public BasicAwaiter<NetworkReplyResult> {
public:
    template<typename REQ, typename RESP>
    explicit NetworkReplyAwaiter(WFNetworkTask<REQ, RESP> *task) {
        using TaskType = WFNetworkTask<REQ, RESP>;

        task->set_callback([info = this->get_info()] (TaskType *task) {
            auto *awaiter = info->get_awaiter<NetworkReplyAwaiter>();
            awaiter->emplace_result(NetworkReplyResult{
                task->get_state(), task->get_error()
            });
            awaiter->done();
        });

        this->set_task(task, true);
    }
};

template<typename REQ, typename RESP>
class ServerContext {
    using ReqType = REQ;
    using RespType = RESP;
    using TaskType = WFNetworkTask<REQ, RESP>;
    using AwaiterType = NetworkReplyAwaiter;

public:
    ServerContext(TaskType *task) : replied(false), task(task)
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

    /**
     * `get_task` returns the `WFNetworkTask *` owned by this ServerContext,
     * to make it easier to use low level functions such as task->get_connection.
     *
     * Read the documentation in detail to learn about lifecycle related issues.
    */
    TaskType *get_task() { return task; }

    AwaiterType reply() {
        // Each ServerContext must reply once
        assert(!replied);
        replied = true;

        return AwaiterType(task);
    }

private:
    bool replied;
    TaskType *task;
};

using ServerParams = WFServerParams;

template<typename REQ, typename RESP>
class BasicServer : public WFServer<REQ, RESP> {
    using BaseType = WFServer<REQ, RESP>;

public:
    using ReqType = REQ;
    using RespType = RESP;
    using TaskType = WFNetworkTask<ReqType, RespType>;
    using ServerContextType = ServerContext<ReqType, RespType>;
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

#endif // COKE_BASIC_SERVER_H
