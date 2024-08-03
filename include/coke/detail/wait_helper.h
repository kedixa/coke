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

#ifndef COKE_DETAIL_WAIT_HELPER_H
#define COKE_DETAIL_WAIT_HELPER_H

#include <cstddef>
#include <type_traits>
#include <vector>
#include <memory>

#include "coke/task.h"
#include "coke/latch.h"

namespace coke::detail {

template<typename T>
struct ValueHelper {
    void set_value(T &&t) {
        value = std::move(t);
    }

    T get_value() {
        return std::move(value);
    }

private:
    T value;
};

template<>
struct ValueHelper<void> {
    void get_value() { }
};

template<typename T>
struct MValueHelper {
    using RetType = std::vector<T>;

    MValueHelper(std::size_t n) {
        value.resize(n);
    }

    void set_value(std::size_t i, T &&t) {
        value[i] = std::move(t);
    }

    std::vector<T> get_value() {
        return std::move(value);
    }

private:
    std::vector<T> value;
};

/**
 * @brief Overloading for bool type because std::vector<bool> cannot be
 *        accessed in multiple threads.
*/
template<>
struct MValueHelper<bool> {
    using RetType = std::vector<bool>;

    MValueHelper(std::size_t n)
        : value(std::make_unique<bool []>(n)), n(n)
    { }

    void set_value(std::size_t i, bool t) {
        value[i] = t;
    }

    std::vector<bool> get_value() {
        return std::vector<bool>(value.get(), value.get() + n);
    }

private:
    std::unique_ptr<bool []> value;
    std::size_t n;
};

template<>
struct MValueHelper<void> {
    using RetType = void;

    MValueHelper(std::size_t) { }

    void get_value() { }
};

template<Cokeable T, typename L>
Task<> coke_wait_helper(Task<T> task, ValueHelper<T> &v, L &lt) {
    if constexpr (std::is_same_v<T, void>)
        co_await task;
    else
        v.set_value(co_await task);

    lt.count_down();
}

template<Cokeable T, typename L>
Task<> coke_wait_helper(Task<T> task, MValueHelper<T> &v, std::size_t i, L &lt)
{
    if constexpr (std::is_same_v<T, void>)
        co_await task;
    else
        v.set_value(i, co_await task);

    lt.count_down();
}

template<Cokeable T>
auto async_wait_helper(std::vector<Task<T>> tasks)
    -> Task<typename MValueHelper<T>::RetType>
{
    std::size_t n = tasks.size();
    Latch lt(n);
    MValueHelper<T> v(n);

    for (std::size_t i = 0; i < n; i++)
        coke_wait_helper(std::move(tasks[i]), v, i, lt).detach();

    co_await lt;
    co_return v.get_value();
}

template<typename T>
concept AwaitableType = requires (T t) {
    requires std::derived_from<T, AwaiterBase>;
    { t.await_resume() };
};

template<typename T>
using AwaiterResult = std::remove_cvref_t<
    std::invoke_result_t<decltype(&T::await_resume), T *>
>;

template<AwaitableType A>
auto make_task_from_awaitable_helper(A a) -> Task<AwaiterResult<A>> {
    co_return co_await a;
}

} // namespace coke::detail

namespace coke {

using detail::AwaitableType;
using detail::AwaiterResult;

template<AwaitableType A>
    requires std::is_same_v<std::remove_reference_t<A>, A>
auto make_task_from_awaitable(A &&a) -> Task<AwaiterResult<A>> {
    return detail::make_task_from_awaitable_helper(std::move(a));
}

} // namespace coke

#endif // COKE_DETAIL_WAIT_HELPER_H
