#ifndef COKE_DETAIL_WAIT_HELPER_H
#define COKE_DETAIL_WAIT_HELPER_H

#include <type_traits>
#include <latch>
#include <vector>
#include <memory>

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
    std::size_t n = tasks.size();
    std::vector<T> vec(n);
    Latch lt((long)n);

    for (std::size_t i = 0; i < n; i++)
        async_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    co_await lt;
    co_return vec;
}

/**
 * Overloading for bool type because std::vector<bool> cannot be accessed by
 * multiple threads.
*/
inline
Task<std::vector<bool>> async_wait_helper(std::vector<Task<bool>> tasks) {
    std::size_t n = tasks.size();
    Latch lt((long)n);
    // `n` == 0 is also valid
    std::unique_ptr<bool []> vec = std::make_unique<bool []>(n);

    for (std::size_t i = 0; i < n; i++)
        async_wait_helper(std::move(tasks[i]), vec[i], lt).start();

    co_await lt;
    co_return std::vector<bool>(vec.get(), vec.get() + n);
}

inline Task<> async_wait_helper(std::vector<Task<void>> tasks) {
    std::size_t n = tasks.size();
    Latch lt((long)n);

    for (std::size_t i = 0; i < n; i++)
        async_wait_helper(std::move(tasks[i]), lt).start();

    co_await lt;
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
