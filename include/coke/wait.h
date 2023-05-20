#ifndef COKE_WAIT_H
#define COKE_WAIT_H

#include <cstddef>
#include <type_traits>
#include <latch>

#include "coke/detail/basic_concept.h"
#include "coke/detail/wait_helper.h"
#include "coke/detail/task.h"

namespace coke {

template<SimpleType T>
T sync_wait(Task<T> &&task) {
    T res;
    std::latch lt(1);

    detail::sync_wait_helper(std::move(task), res, lt).start();
    lt.wait();
    return res;
}

inline void sync_wait(Task<void> &&task) {
    std::latch lt(1);

    detail::sync_wait_helper(std::move(task), lt).start();
    lt.wait();
}

template<SimpleType T>
std::vector<T> sync_wait(std::vector<Task<T>> &&tasks) {
    size_t n = tasks.size();
    std::vector<T> vec(n);
    std::latch lt(n);

    for (size_t i = 0; i < n; i++)
        detail::sync_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    lt.wait();
    return vec;
}

inline void sync_wait(std::vector<Task<void>> &&tasks) {
    size_t n = tasks.size();
    std::latch lt(n);

    for (size_t i = 0; i < n; i++)
        detail::sync_wait_helper(std::move(tasks[i]), lt).start();

    lt.wait();
}

template<SimpleType T, SimpleType... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...> && !std::is_same_v<T, void>)
std::vector<T> sync_wait(Task<T> &&first, Task<Ts>&&... others) {
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return sync_wait(std::move(tasks));
}

template<SimpleType... Ts>
    requires std::conjunction_v<std::is_same<void, Ts>...>
void sync_wait(Task<> &&first, Task<Ts>&&... others) {
    std::vector<Task<>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return sync_wait(std::move(tasks));
}


template<SimpleType T, SimpleType... Ts>
    requires (std::conjunction_v<std::is_same<T, Ts>...> && !std::is_same_v<T, void>)
Task<std::vector<T>> async_wait(Task<T> &&first, Task<Ts>&&... others) {
    std::vector<Task<T>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return detail::async_wait_helper(std::move(tasks));
}

template<SimpleType... Ts>
    requires std::conjunction_v<std::is_same<void, Ts>...>
Task<> async_wait(Task<> &&first, Task<Ts>&&... others) {
    std::vector<Task<>> tasks;
    tasks.reserve(sizeof...(Ts) + 1);
    tasks.emplace_back(std::move(first));
    (tasks.emplace_back(std::move(others)), ...);

    return detail::async_wait_helper(std::move(tasks));
}

template<SimpleType T>
Task<std::vector<T>> async_wait(std::vector<Task<T>> &&tasks) {
    return detail::async_wait_helper(std::move(tasks));
}

} // namespace coke

#endif // COKE_WAIT_H
