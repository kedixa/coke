#ifndef COKE_NETWORK_H
#define COKE_NETWORK_H

#include <optional>
#include <utility>
#include <cassert>

#include "coke/detail/awaiter_base.h"
#include "workflow/WFTask.h"

namespace coke {

template<typename REQ, typename RESP>
struct NetworkResult {
    using ReqType = REQ;
    using RespType = RESP;
    using TaskType = WFNetworkTask<ReqType, RespType>;

    int state;
    int error;

    RespType resp;

    // Attention: the task will be destroyed at the beginning of the next task
    TaskType *task;
};

template<typename REQ, typename RESP>
class NetworkAwaiter : public BasicAwaiter<NetworkResult<REQ, RESP>> {
public:
    using ReqType = REQ;
    using RespType = RESP;
    using ResultType = NetworkResult<ReqType, RespType>;
    using TaskType = WFNetworkTask<ReqType, RespType>;

public:
    explicit NetworkAwaiter(TaskType *task, bool move_resp = true) {
        task->set_callback([info = this->get_info(), move_resp] (TaskType *task) {
            using AwaiterType = NetworkAwaiter<ReqType, RespType>;
            auto *awaiter = info->template get_awaiter<AwaiterType>();

            ResultType result;
            result.state = task->get_state();
            result.error = task->get_error();
            result.task = task;

            if (move_resp)
                result.resp = std::move(*task->get_resp());

            awaiter->emplace_result(std::move(result));
            awaiter->done();
        });

        this->set_task(task);
    }
};

} // namespace coke

#endif // COKE_NETWORK_H
