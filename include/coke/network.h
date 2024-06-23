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
class [[nodiscard]] NetworkAwaiter
    : public BasicAwaiter<NetworkResult<REQ, RESP>> {
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

/**
 * @brief This is a simpler network task awaiter that can avoid unnecessary
 * construction and movement of the response. Please understand the life cycle
 * of the task in detail before using it.
*/
template<typename REQ, typename RESP>
class SimpleNetworkAwaiter : public BasicAwaiter<WFNetworkTask<REQ,RESP> *> {
public:
    using ReqType = REQ;
    using RespType = RESP;
    using TaskType = WFNetworkTask<ReqType, RespType>;
    using ResultType = TaskType *;

public:
    explicit SimpleNetworkAwaiter(TaskType *task) {
        task->set_callback([info = this->get_info()] (TaskType *task) {
            using AwaiterType = SimpleNetworkAwaiter<ReqType, RespType>;
            auto *awaiter = info->template get_awaiter<AwaiterType>();

            awaiter->emplace_result(task);
            awaiter->done();
        });

        this->set_task(task);
    }
};

} // namespace coke

#endif // COKE_NETWORK_H
