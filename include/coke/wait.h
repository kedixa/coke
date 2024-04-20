#ifndef COKE_WAIT_H
#define COKE_WAIT_H

#include <cstddef>
#include <type_traits>
#include <memory>
#include <latch>

#include "coke/global.h"
#include "coke/detail/wait_helper.h"

namespace coke {

template<Cokeable T>
T sync_wait(Task<T> &&task) {
    T res;
    SyncLatch lt(1);

    detail::sync_wait_helper(std::move(task), res, lt).start();
    lt.wait();
    return res;
}

inline void sync_wait(Task<void> &&task) {
    SyncLatch lt(1);

    detail::sync_wait_helper(std::move(task), lt).start();
    lt.wait();
}

template<Cokeable T>
std::vector<T> sync_wait(std::vector<Task<T>> &&tasks) {
    std::size_t n = tasks.size();
    std::vector<T> vec(n);
    SyncLatch lt(n);

    for (std::size_t i = 0; i < n; i++)
        detail::sync_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    lt.wait();
    return vec;
}

/**
 * Overloading for bool type because std::vector<bool> cannot be accessed by
 * multiple threads.
*/
inline
std::vector<bool> sync_wait(std::vector<Task<bool>> &&tasks) {
    std::size_t n = tasks.size();
    SyncLatch lt(n);
    // `n` == 0 is also valid
    std::unique_ptr<bool []> vec = std::make_unique<bool []>(n);

    for (std::size_t i = 0; i < n; i++)
        detail::sync_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    lt.wait();
    return std::vector<bool>(vec.get(), vec.get() + n);
}

inline void sync_wait(std::vector<Task<void>> &&tasks) {
    std::size_t n = tasks.size();
    SyncLatch lt(n);

    for (std::size_t i = 0; i < n; i++)
        detail::sync_wait_helper(std::move(tasks[i]), lt).start();

    lt.wait();
}

template<Cokeable T, Cokeable... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...> && !std::is_same_v<T, void>)
std::vector<T> sync_wait(Task<T> &&first, Task<Ts>&&... others) {
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return sync_wait(std::move(tasks));
}

template<Cokeable... Ts>
    requires std::conjunction_v<std::is_same<void, Ts>...>
void sync_wait(Task<> &&first, Task<Ts>&&... others) {
    std::vector<Task<>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return sync_wait(std::move(tasks));
}

template<AwaitableType A>
auto sync_wait(A &&a) -> AwaiterResult<A> {
    return sync_wait(make_task_from_awaitable(std::move(a)));
}

template<AwaitableType A, AwaitableType... As>
    requires std::conjunction_v<std::is_same<AwaiterResult<A>, AwaiterResult<As>>...>
auto sync_wait(A &&first, As&&... others) {
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(sizeof...(As) + 1);
    tasks.emplace_back(make_task_from_awaitable(std::move(first)));
    (tasks.emplace_back(make_task_from_awaitable(std::move(others))), ...);

    return sync_wait(std::move(tasks));
}

template<AwaitableType A>
auto sync_wait(std::vector<A> &&as) {
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(as.size());
    for (std::size_t i = 0; i < as.size(); i++)
        tasks.emplace_back(make_task_from_awaitable(std::move(as[i])));

    return sync_wait(std::move(tasks));
}


template<Cokeable T, Cokeable... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...> && !std::is_same_v<T, void>)
[[nodiscard]]
Task<std::vector<T>> async_wait(Task<T> &&first, Task<Ts>&&... others) {
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return detail::async_wait_helper(std::move(tasks));
}

template<Cokeable... Ts>
    requires std::conjunction_v<std::is_same<void, Ts>...>
[[nodiscard]]
Task<> async_wait(Task<> &&first, Task<Ts>&&... others) {
    std::vector<Task<>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return detail::async_wait_helper(std::move(tasks));
}

template<Cokeable T>
[[nodiscard]]
Task<std::vector<T>> async_wait(std::vector<Task<T>> &&tasks) {
    return detail::async_wait_helper(std::move(tasks));
}

[[nodiscard]]
inline Task<> async_wait(std::vector<Task<void>> &&tasks) {
    return detail::async_wait_helper(std::move(tasks));
}

template<AwaitableType A, AwaitableType... As>
    requires std::conjunction_v<std::is_same<AwaiterResult<A>, AwaiterResult<As>>...>
[[nodiscard]]
auto async_wait(A &&first, As&&... others) {
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(sizeof...(As) + 1);
    tasks.emplace_back(make_task_from_awaitable(std::move(first)));
    (tasks.emplace_back(make_task_from_awaitable(std::move(others))), ...);

    return detail::async_wait_helper(std::move(tasks));
}

template<AwaitableType A>
[[nodiscard]]
auto async_wait(std::vector<A> &&as) {
    using return_type = AwaiterResult<A>;

    std::vector<Task<return_type>> tasks;
    tasks.reserve(as.size());
    for (std::size_t i = 0; i < as.size(); i++)
        tasks.emplace_back(make_task_from_awaitable(std::move(as[i])));

    return detail::async_wait_helper(std::move(tasks));
}

} // namespace coke

#endif // COKE_WAIT_H
