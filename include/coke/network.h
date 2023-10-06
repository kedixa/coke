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

} // namespace coke

#endif // COKE_NETWORK_H
