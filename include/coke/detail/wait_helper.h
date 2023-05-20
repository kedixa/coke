#ifndef COKE_DETAIL_WAIT_HELPER_H
#define COKE_DETAIL_WAIT_HELPER_H

#include <type_traits>
#include <latch>
#include <vector>

#include "coke/detail/task.h"
#include "coke/latch.h"

namespace coke::detail {

template<SimpleType T>
Task<> sync_wait_helper(Task<T> task, T &res, std::latch &lt) {
    res = co_await task;
    lt.count_down();
}

inline Task<> sync_wait_helper(Task<void> task, std::latch &lt) {
    co_await task;
    lt.count_down();
}


template<SimpleType T>
Task<> async_wait_helper(Task<T> task, T &res, Latch &lt) {
    res = co_await task;
    lt.count_down();
}

inline Task<> async_wait_helper(Task<void> task, Latch &lt) {
    co_await task;
    lt.count_down();
}

template<SimpleType T>
Task<std::vector<T>> async_wait_helper(std::vector<Task<T>> tasks) {
    size_t n = tasks.size();
    std::vector<T> vec(n);
    Latch lt(n);

    for (size_t i = 0; i < n; i++)
        async_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    co_await lt;
    co_return vec;
}

inline Task<> async_wait_helper(std::vector<Task<void>> tasks) {
    size_t n = tasks.size();
    Latch lt(n);

    for (size_t i = 0; i < n; i++)
        async_wait_helper(std::move(tasks[i]), lt).start();

    co_await lt;
}

} // namespace coke::detail

#endif // COKE_DETAIL_WAIT_HELPER_H