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

#ifndef COKE_WAIT_H
#define COKE_WAIT_H

#include "coke/detail/wait_helper.h"
#include "coke/make_task.h"

namespace coke {

/**
 * @brief Sync wait for single coke::Task<T>.
 * @return T.
 */
template<Cokeable T>
T sync_wait(Task<T> &&task)
{
    SyncLatch lt(1);
    detail::ValueHelper<T> v;

    detail::coke_wait_helper(std::move(task), v, lt).detach();
    lt.wait();
    return v.get_value();
}

/**
 * @brief Sync wait for a vector of coke::Task<T>.
 * @return std::vector<T> if T is not void, else void.
 */
template<Cokeable T>
auto sync_wait(std::vector<Task<T>> &&tasks)
{
    std::size_t n = tasks.size();
    SyncLatch lt(n);
    detail::MValueHelper<T> v(n);

    for (std::size_t i = 0; i < n; i++)
        detail::coke_wait_helper(std::move(tasks[i]), v, i, lt).detach();

    lt.wait();
    return v.get_value();
}

/**
 * @brief Sync wait for a constant number of coke::Task<T>.
 * @return std::vector<T> if T is not void, else void.
 * @attention It is not recommended to use this function when T is not void
 *            because the return type is inappropriate.
 */
template<Cokeable T, Cokeable... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...>)
auto sync_wait(Task<T> &&first, Task<Ts> &&...others)
{
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return sync_wait(std::move(tasks));
}

/**
 * @brief Sync wait for a awaitable object derived from coke::AwaiterBase.
 */
template<AwaitableType A>
auto sync_wait(A &&a) -> AwaiterResult<A>
{
    return sync_wait(make_task_from_awaitable(std::move(a)));
}

/**
 * @brief Sync wait for a constant number of awaitable object.
 */
template<AwaitableType A, AwaitableType... As>
    requires std::conjunction_v<
        std::is_same<AwaiterResult<A>, AwaiterResult<As>>...>
auto sync_wait(A &&first, As &&...others)
{
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(sizeof...(As) + 1);
    tasks.emplace_back(make_task_from_awaitable(std::move(first)));
    (tasks.emplace_back(make_task_from_awaitable(std::move(others))), ...);

    return sync_wait(std::move(tasks));
}

/**
 * @brief Sync wait for a vector of awaitable object.
 */
template<AwaitableType A>
auto sync_wait(std::vector<A> &&as)
{
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(as.size());
    for (std::size_t i = 0; i < as.size(); i++)
        tasks.emplace_back(make_task_from_awaitable(std::move(as[i])));

    return sync_wait(std::move(tasks));
}

/**
 * @brief Async wait for a vector of coke::Task<T>.
 * @return std::vector<T> if T is not void, else void.
 */
template<Cokeable T>
auto async_wait(std::vector<Task<T>> &&tasks)
{
    return detail::async_wait_helper(std::move(tasks));
}

/**
 * @brief Async wait for a constant number of coke::Task<T>.
 * @return std::vector<T> if T is not void, else void.
 * @attention It is not recommended to use this function when T is not void
 *            because the return type is inappropriate.
 */
template<Cokeable T, Cokeable... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...>)
auto async_wait(Task<T> &&first, Task<Ts> &&...others)
    -> Task<typename detail::MValueHelper<T>::RetType>
{
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return detail::async_wait_helper(std::move(tasks));
}

/**
 * @brief Async wait for a constant number of awaitable object.
 */
template<AwaitableType A, AwaitableType... As>
    requires std::conjunction_v<
        std::is_same<AwaiterResult<A>, AwaiterResult<As>>...>
auto async_wait(A &&first, As &&...others)
{
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(sizeof...(As) + 1);
    tasks.emplace_back(make_task_from_awaitable(std::move(first)));
    (tasks.emplace_back(make_task_from_awaitable(std::move(others))), ...);

    return detail::async_wait_helper(std::move(tasks));
}

/**
 * @brief Async wait for a vector of awaitable object.
 */
template<AwaitableType A>
auto async_wait(std::vector<A> &&as)
{
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(as.size());
    for (std::size_t i = 0; i < as.size(); i++)
        tasks.emplace_back(make_task_from_awaitable(std::move(as[i])));

    return detail::async_wait_helper(std::move(tasks));
}

/**
 * @brief Make task func(args...) and sync wait.
 */
template<typename FUNC, typename... ARGS>
auto sync_call(FUNC &&func, ARGS &&...args)
{
    return sync_wait(
        make_task(std::forward<FUNC>(func), std::forward<ARGS>(args)...));
}

} // namespace coke

#endif // COKE_WAIT_H
