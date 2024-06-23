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

#ifndef COKE_SERIES_H
#define COKE_SERIES_H

#include "coke/detail/series_task.h"

namespace coke {

/**
 * @brief SeriesAwaiter is not an awaiter that waits for the SeriesWork to
 *        finish, but get the current running SeriesWork.
 *
 * Do not use SeriesAwaiter directly, but use `coke::current_series` like
 * `SeriesWork *series = co_await coke::current_series();`.
*/
class [[nodiscard]] SeriesAwaiter final : public AwaiterBase {
public:
    SeriesAwaiter() : series_task(detail::create_series_task()) {
        series_task->set_awaiter(this);
        this->set_task(series_task);
    }

    SeriesAwaiter(SeriesAwaiter &&that)
        : AwaiterBase(std::move(that)), series_task(that.series_task)
    {
        that.series_task = nullptr;

        if (series_task)
            series_task->set_awaiter(this);
    }

    SeriesAwaiter &operator= (SeriesAwaiter &&that) {
        if (this != &that) {
            AwaiterBase::operator=(std::move(that));
            std::swap(this->series_task, that.series_task);

            if (series_task)
                series_task->set_awaiter(this);
            if (that.series_task)
                that.series_task->set_awaiter(&that);
        }

        return *this;
    }

    ~SeriesAwaiter() = default;

    SeriesWork *await_resume() { return series_of(series_task); }

private:
    detail::SeriesTask *series_task;

    friend SeriesAwaiter current_series();
    friend SeriesAwaiter empty();
};

/**
 * @brief ParallelAwaiter is an awaiter that waits for the ParallelWork to
 *        finish. Use `coke::wait_parallel` directly.
*/
class [[nodiscard]] ParallelAwaiter : public BasicAwaiter<const ParallelWork *>
{
private:
    ParallelAwaiter(ParallelWork *par) {
        auto *info = this->get_info();
        par->set_callback([info](const ParallelWork *par) {
            auto *awaiter = info->get_awaiter<ParallelAwaiter>();
            awaiter->emplace_result(par);
            awaiter->done();
        });

        this->set_task(par);
    }

    friend ParallelAwaiter wait_parallel(ParallelWork *);
};


using EmptyAwaiter = SeriesAwaiter;

/**
 * @brief Get the SeriesWork which is running on this coroutine now.
 *
 * If not, a new SeriesWork will be created by the series creater registed
 * by coke::set_series_creater or by the default series creater.
*/
inline SeriesAwaiter current_series() { return SeriesAwaiter(); }

/**
 * @brief Same as coke::current_series(), just to have a WFEmptyTask
 *        corresponding semantics.
*/
inline EmptyAwaiter empty() { return EmptyAwaiter(); }

/**
 * @brief Run and wait the parallel work to finish, it's callback is used
 *        by coke, so you cannot set callback to the parallel work.
*/
inline ParallelAwaiter wait_parallel(ParallelWork *parallel) {
    return ParallelAwaiter(parallel);
}

/**
 * @brief A function type that can be used to create a new series.
*/
using SeriesCreater = SeriesWork * (*)(SubTask *);

/**
 * @brief Set a new series creater and return the old one.
 *
 * This function must be called exactly before any coroutine is created.
 *
 * @param creater An instance of SeriesCreater which don't throw any exceptions.
 *        If creater is nullptr, default series creater is set.
 * @return Previous series creater, if this is the first call, default series
 *         creater is returned.
*/
SeriesCreater set_series_creater(SeriesCreater creater) noexcept;

/**
 * @brief Get current series creater.
 *
 * @return Same as the param of last call to `set_series_creater`, if it is
 *         never called, return default series creater.
*/
SeriesCreater get_series_creater() noexcept;

/**
 * @brief Detach the coke::Task on the running series. Each valid coke::Task
 *        can be co awaited, detached, detached on series, detached on new
 *        series at most once.
 * @param task Valid coke::Task object.
 * @param series A running series.
*/
template<Cokeable T>
void detach_on_series(Task<T> &&task, SeriesWork *series) {
    task.detach_on_series(series);
}

/**
 * @brief Detach the coke::Task on new series.  Each valid coke::Task
 *        can be co awaited, detached, detached on series, detached on new
 *        series at most once.
 * @param task Valid coke::Task object.
 * @param creater Callable object that accepts a SubTask * parameter and
 *        returns a newly created SeriesWork.
*/
template<Cokeable T, typename SeriesCreaterType>
void detach_on_new_series(Task<T> &&task, SeriesCreaterType &&creater) {
    SubTask *detach_task = detail::create_detach_task(std::move(task));
    SeriesWork *series = creater(detach_task);
    series->start();
}

} // namespace coke

#endif // COKE_SERIES_H
