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

#ifndef COKE_DETAIL_AWAITER_BASE_H
#define COKE_DETAIL_AWAITER_BASE_H

#include <coroutine>
#include <utility>

#include "coke/detail/basic_concept.h"
#include "workflow/SubTask.h"

namespace coke {

/**
 * @brief Common base class for awaitable objects in coke project.
 *
 * Awaitable objects are used to wait for a workflow task and notify the caller
 * of its results.
*/
class AwaiterBase {
    static void *create_series(SubTask *first);

public:
    constexpr static bool __is_coke_awaitable_type = true;

    AwaiterBase() = default;

    /**
     * @brief If the task associated with the current awaiter has not been
     *        started, the default action of the destructor is to delete task.
    */
    virtual ~AwaiterBase();

    /**
     * @brief AwaiterBase is not copyable.
    */
    AwaiterBase &operator= (const AwaiterBase &) = delete;

    AwaiterBase &operator=(AwaiterBase &&that) {
        if (this != &that) {
            std::swap(this->hdl, that.hdl);
            std::swap(this->subtask, that.subtask);
            std::swap(this->in_series, that.in_series);
        }

        return *this;
    }

    AwaiterBase(AwaiterBase &&that)
        : hdl(that.hdl), subtask(that.subtask), in_series(that.in_series)
    {
        that.hdl = nullptr;
        that.subtask = nullptr;
        that.in_series = false;
    }

    /**
     * @brief Return whether this awaiter is already ready.
     *
     * If the awaiter not maintains a task, it will not be suspend.
    */
    bool await_ready() { return subtask == nullptr; }

    /**
     * @brief Suspend current coroutine h and start the task(maintained by this
     *        awaiter). If PromiseType is CoPromise, task will run on the same
     *        series in h, otherwise on a new series.
     * @tparam PromiseType Type of promise.
    */
    template<typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        this->hdl = h;

        if constexpr (IsCokePromise<PromiseType>) {
            // The Awaiter is awaited in CoPromise
            void *series = h.promise().get_series();

            if (series)
                suspend(series);
            else {
                series = create_series(subtask);
                h.promise().set_series(series);
                suspend(series, true);
            }
        }
        else {
            // The Awaiter is awaited in third party coroutine framework
            void *series = create_series(subtask);
            suspend(series, true);
        }

        // `*this` maybe destroyed after suspend
    }

    /**
     * @brief This function is only for developers, we make it public just
     *        for convenience, and users should not call it directly.
     *        When this->subtask finishes, call this->done() to wake up the
     *        coroutine.
     *
     * @pre this->subtask is done.
    */
    virtual void done() {
        subtask = nullptr;
        hdl.resume();
    }

protected:
    /**
     * @brief The `suspend` will be called in `await_suspend`, child classes
     *        can override this function to do something else.
     *
     * @param series The coroutine is running on this series.
     * @param is_new Whether this is the first awaiter in this coroutine. If
     *        `is_new` is true, this->subtask is the first task of the series,
     *        and the series is not started now.
    */
    virtual void suspend(void *series, bool is_new = false);

    /**
     * @brief Set `subtask` to this awaiter, `set_task` can be called at most
     *        once, if not, co_await this awaiter will do nothing.
     *
     * @param subtask An instance of SubTask's child class.
     * @param in_series Whether `subtask` is already in current series, if true,
     *        it will not push back to series again, for example, reply_task is
     *        in the server's series.
     *        It is bad behavious if `in_series` is true but not in current
     *        coroutine's series.
    */
    void set_task(SubTask *subtask, bool in_series = false) noexcept {
        this->subtask = subtask;
        this->in_series = in_series;
    }

protected:
    std::coroutine_handle<> hdl;
    SubTask *subtask{nullptr};
    bool in_series{false};
};

} // namespace coke

#endif // COKE_DETAIL_AWAITER_BASE_H
