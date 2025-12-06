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

#ifndef COKE_GO_H
#define COKE_GO_H

#include <string_view>

#include "coke/detail/go_task.h"

namespace coke {

/**
 * @brief Default ExecQueue name when use coke::go without name.
 */
inline constexpr std::string_view GO_DEFAULT_QUEUE{"coke:go"};

template<typename T>
class [[nodiscard]] GoAwaiter : public AwaiterBase {
public:
    template<typename FUNC, typename... ARGS>
        requires std::invocable<FUNC, ARGS...>
    GoAwaiter(ExecQueue *queue, Executor *executor, FUNC &&func, ARGS &&...args)
    {
        std::function<T()> go([func = std::forward<FUNC>(func),
                               ... args = std::forward<ARGS>(args)]() mutable {
            return std::invoke(func, std::unwrap_reference_t<ARGS>(args)...);
        });

        this->go_task = new detail::GoTask<T>(queue, executor, std::move(go));
        go_task->set_awaiter(this);

        this->set_task(go_task);
    }

    // only for switch_go_thread
    GoAwaiter(ExecQueue *queue, Executor *executor)
        requires std::is_same_v<T, void>
    {
        this->go_task = new detail::GoTask<T>(queue, executor, nullptr);
        go_task->set_awaiter(this);
        this->set_task(go_task);
    }

    GoAwaiter(GoAwaiter &&that) noexcept
        : AwaiterBase(std::move(that)),
          go_task(std::exchange(that.go_task, nullptr))
    {
        if (this->go_task)
            this->go_task->set_awaiter(this);
    }

    GoAwaiter &operator=(GoAwaiter &&that) noexcept
    {
        if (this != &that) {
            this->AwaiterBase::operator=(std::move(that));
            std::swap(this->go_task, that.go_task);

            if (this->go_task)
                this->go_task->set_awaiter(this);

            if (that.go_task)
                that.go_task->set_awaiter(&that);
        }

        return *this;
    }

    T await_resume()
    {
        if constexpr (!std::is_same_v<T, void>)
            return std::move(go_task->get_result());
    }

private:
    detail::GoTask<T> *go_task;
};

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(ExecQueue *queue, Executor *executor, FUNC &&func, ARGS &&...args)
{
    using result_t = std::remove_cvref_t<std::invoke_result_t<FUNC, ARGS...>>;

    return GoAwaiter<result_t>(queue, executor, std::forward<FUNC>(func),
                               std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(const std::string &name, FUNC &&func, ARGS &&...args)
{
    auto *queue = detail::get_exec_queue(name);
    auto *executor = detail::get_compute_executor();

    return go(queue, executor, std::forward<FUNC>(func),
              std::forward<ARGS>(args)...);
}

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto go(FUNC &&func, ARGS &&...args)
{
    return go(std::string(GO_DEFAULT_QUEUE), std::forward<FUNC>(func),
              std::forward<ARGS>(args)...);
}

/**
 * @brief Switch to a thread with `queue` and `executor`
 */
inline auto switch_go_thread(ExecQueue *queue, Executor *executor)
{
    return GoAwaiter<void>(queue, executor);
}

/**
 * @brief Switch to a thread in the compute thread pool with `name`
 */
inline auto switch_go_thread(const std::string &name)
{
    auto *queue = detail::get_exec_queue(name);
    auto *executor = detail::get_compute_executor();
    return switch_go_thread(queue, executor);
}

/**
 * @brief Switch to a thread in the compute thread pool with default name
 */
inline auto switch_go_thread()
{
    return switch_go_thread(std::string(GO_DEFAULT_QUEUE));
}

} // namespace coke

#endif // COKE_GO_H
