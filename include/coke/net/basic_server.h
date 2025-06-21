/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#ifndef COKE_BASIC_SERVER_H
#define COKE_BASIC_SERVER_H

#include "coke/net/network.h"
#include "coke/task.h"
#include "workflow/WFServer.h"

namespace coke {

struct NetworkReplyResult {
    int state;
    int error;
};

class [[nodiscard]] NetworkReplyAwaiter
    : public BasicAwaiter<NetworkReplyResult> {
public:
    template<typename REQ, typename RESP>
    explicit NetworkReplyAwaiter(WFNetworkTask<REQ, RESP> *task)
    {
        using TaskType = WFNetworkTask<REQ, RESP>;

        task->set_callback([info = this->get_info()](TaskType *task) {
            int state = task->get_state();
            int error = task->get_error();

            auto *awaiter = info->get_awaiter<NetworkReplyAwaiter>();
            awaiter->emplace_result(NetworkReplyResult{state, error});
            awaiter->done();
        });

        this->set_task(task, true);
    }
};

template<typename REQ, typename RESP>
class ServerContext {
    using ReqType     = REQ;
    using RespType    = RESP;
    using TaskType    = WFNetworkTask<REQ, RESP>;
    using AwaiterType = NetworkReplyAwaiter;

public:
    ServerContext(TaskType *task) : replied(false), task(task) {}

    ServerContext(ServerContext &&that) : replied(that.replied), task(that.task)
    {
        // that cannot be used any more
        that.replied = true;
        that.task    = nullptr;
    }

    ServerContext &operator=(ServerContext &&that)
    {
        if (this != &that) {
            std::swap(this->task, that.task);
            std::swap(this->replied, that.replied);
        }

        return *this;
    }

    ServerContext &operator=(const ServerContext &) = delete;

    ReqType &get_req() & { return *(task->get_req()); }
    RespType &get_resp() & { return *(task->get_resp()); }
    long long get_seqid() { return task->get_seq(); }

    /**
     * @brief Returns the server task owned by this ServerContext, make it
     *        easier to use its member functions such as task->get_connection.
     * @attention Make sure you understand the workflow task's lifecycle.
     */
    TaskType *get_task() { return task; }

    AwaiterType reply()
    {
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
    using ReqType           = REQ;
    using RespType          = RESP;
    using TaskType          = WFNetworkTask<ReqType, RespType>;
    using ServerContextType = ServerContext<ReqType, RespType>;
    using ReplyResultType   = NetworkReplyResult;
    using ProcessorType     = std::function<Task<>(ServerContextType)>;

private:
    static auto get_process(BasicServer *server)
    {
        return [server](TaskType *task) {
            return server->do_proc(task);
        };
    }

public:
    BasicServer(const ServerParams &params, ProcessorType co_proc)
        : WFServer<REQ, RESP>(&params, get_process(this)),
          co_proc(std::move(co_proc))
    {
    }

protected:
    virtual void do_proc(TaskType *task)
    {
        Task<> t = co_proc(ServerContextType(task));
        t.detach_on_series(series_of(task));
    }

private:
    ProcessorType co_proc;
};

namespace detail {

template<typename P>
ServerParams to_server_params(const P &p) noexcept
{
    ServerParams r = SERVER_PARAMS_DEFAULT;

    r.transport_type        = p.transport_type;
    r.max_connections       = p.max_connections;
    r.peer_response_timeout = p.peer_response_timeout;
    r.receive_timeout       = p.receive_timeout;
    r.keep_alive_timeout    = p.keep_alive_timeout;
    r.request_size_limit    = p.request_size_limit;
    r.ssl_accept_timeout    = p.ssl_accept_timeout;

    return r;
}

} // namespace detail

} // namespace coke

#endif // COKE_BASIC_SERVER_H
