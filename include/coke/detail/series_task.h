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

#ifndef COKE_DETAIL_SERIES_TASK_H
#define COKE_DETAIL_SERIES_TASK_H

#include "coke/detail/awaiter_base.h"
#include "coke/task.h"
#include "workflow/Workflow.h"

namespace coke::detail {

template<Cokeable T>
class DetachTask final : public SubTask {
public:
    DetachTask(Task<T> &&task) : task(std::move(task)) { }

protected:
    virtual void dispatch() override {
        this->subtask_done();
    }

    virtual SubTask *done() override {
        SeriesWork *series = series_of(this);
        task.detach_on_series(series);

        delete this;
        return series->pop();
    }

private:
    Task<T> task;
};

class SeriesTask final : public SubTask {
public:
    SeriesTask() noexcept : awaiter(nullptr) { }

    AwaiterBase *get_awaiter() const noexcept { return awaiter; }
    void set_awaiter(AwaiterBase *awaiter) noexcept { this->awaiter = awaiter; }

private:
    virtual void dispatch() override {
        this->subtask_done();
    }

    virtual SubTask *done() override {
        SeriesWork *series = series_of(this);
        awaiter->done();

        delete this;
        return series->pop();
    }

private:
    AwaiterBase *awaiter;
};

// Create tasks.

template<Cokeable T>
auto create_detach_task(Task<T> &&task) {
    return new DetachTask<T>(std::move(task));
}

inline auto create_series_task() {
    return new SeriesTask();
}

} // namespace coke::detail

#endif // COKE_DETAIL_SERIES_TASK_H
