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

#ifndef COKE_FUTURE_H
#define COKE_FUTURE_H

#include "coke/detail/future_base.h"
#include "coke/series.h"

namespace coke {

template<Cokeable Res>
class Future {
    using State = detail::FutureState<Res>;

public:
    Future() = default;
    Future(Future &&) = default;
    Future &operator=(Future &&) = default;
    ~Future() = default;

    Future(const Future &) = delete;
    Future &operator=(const Future &) = delete;

    /**
     * @brief Return whether the state of this Future is valid.
     *
     * A default constructed Future is not valid, and a Future get from Promise
     * is valid.
     */
    bool valid() const noexcept { return (bool)state; }

    /**
     * @brief Get the internal state of this future. See the global constants
     *        named coke::FUTURE_STATE_XXX.
     */
    int get_state() const noexcept { return state->get_state(); }

    /**
     * @brief Return whether the state of this Future is ready.
     * @pre valid() returns true.
     */
    bool ready() const noexcept
    {
        return state->get_state() == FUTURE_STATE_READY;
    }

    /**
     * @brief Return whether the promise destroyed without set value.
     * @pre valid() returns true.
     */
    bool broken() const noexcept
    {
        return state->get_state() == FUTURE_STATE_BROKEN;
    }

    /**
     * @brief Return whether there is an exception.
     * @pre valid() returns true.
     */
    bool has_exception() const noexcept
    {
        return state->get_state() == FUTURE_STATE_EXCEPTION;
    }

    /**
     * @brief Blocks until ready, broken or exception.
     * @pre valid() returns true.
     *
     * See wait_for() for more infomation, but ignore FUTURE_STATE_TIMEOUT.
     */
    Task<int> wait() { return state->wait(); }

    /**
     * @brief Blocks until ready, broken, exception or timeout.
     * @pre valid() returns true.
     *
     * @returns coke::Task<int> that should be awaited immediately.
     * @retval coke::FUTURE_STATE_BROKEN if promise destroyed without set value.
     * @retval coke::FUTURE_STATE_READY if state is ready.
     * @retval coke::FUTURE_STATE_EXCEPTION if there is an exception.
     * @retval coke::FUTURE_STATE_TIMEOUT if wait timeout.
     * @retval coke::FUTURE_STATE_ABORTED if wait is aborted by process exit.
     *
     * It is better to ensure that all coroutines finish before process exits.
     */
    Task<int> wait_for(const NanoSec &nsec) { return state->wait_for(nsec); }

    /**
     * @brief Notify the associated Promise to cancel the current process.
     * @pre valid() returns true.
     */
    void cancel() { state->set_canceled(); }

    /**
     * @brief Gets the value set by the Promise associated with the Future.
     *        If there is an exception set by Promise, it will be rethrow.
     *        This function can only be called at most once.
     *
     * @pre ready() returns true.
     * @post The saved value is in an undefined state.
     */
    Res get()
    {
        if constexpr (!std::is_void_v<Res>)
            return std::move(state->get());
        else
            return state->get();
    }

    /**
     * @brief Get the exception associated with the Future.
     * @pre exception() returns true.
     */
    std::exception_ptr get_exception() { return state->get_exception(); }

    /**
     * @brief Sets a callback that will be called once when the future is
     *        completed, or immediately if it is already completed.
     *
     * The int param of the callback is the value of this->get_state().
     *
     * @attention This is mainly for internal use, and you can also use it if
     *            understand how to use. See coke::wait_futures.
     */
    void set_callback(std::function<void(int)> callback)
    {
        state->set_callback(std::move(callback));
    }

    /**
     * @brief Remove the callback.
     */
    void remove_callback() { state->remove_callback(); }

private:
    /**
     * @brief Future can only be created by Promise.
     */
    Future(std::shared_ptr<State> ptr) noexcept : state(ptr) {}

    friend Promise<Res>;

private:
    std::shared_ptr<State> state;
}; // class Future

template<Cokeable Res>
class Promise {
    using State = detail::FutureState<Res>;

public:
    Promise() { state = std::make_shared<State>(); }

    ~Promise()
    {
        if (state && state->get_state() == FUTURE_STATE_NOTSET)
            state->set_broken();
    }

    Promise(Promise &&) = default;
    Promise &operator=(Promise &&) = default;

    Promise(const Promise &) = delete;
    Promise &operator=(const Promise &) = delete;

    /**
     * @brief Get the Future associated with this Promise. This function can
     *        only be called at most once.
     */
    Future<Res> get_future() { return Future<Res>(state); }

    /**
     * @brief Set value and wake up Future.
     * @exception When copy constructor of Res throws.
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
     */
    template<typename U>
        requires std::is_same_v<Res, std::remove_cvref_t<U>>
    bool set_value(U &&value)
    {
        return state->set_value(std::forward<U>(value));
    }

    /**
     * @brief Set value and wake up Future. Specialization when Res is void.
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
     */
    bool set_value()
        requires std::is_same_v<Res, void>
    {
        return state->set_value();
    }

    /**
     * @brief Set exception and wake up Future.
     * @exception When copy constructor of std::exception_ptr throws.
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
     */
    bool set_exception(const std::exception_ptr &eptr)
    {
        return state->set_exception(eptr);
    }

    /**
     * @brief Check whether `cancel` is called by the associated Future.
     */
    bool is_canceled() const noexcept { return state->is_canceled(); }

private:
    std::shared_ptr<State> state;
};

/**
 * @brief Create coke::Future<T> from coke::Task<T> on running series, the task
 *        will be started immediately.
 */
template<Cokeable T>
Future<T> create_future(Task<T> &&task, SeriesWork *series)
{
    Promise<T> promise;
    Future<T> future = promise.get_future();

    Task<> t = detail::detach_task(std::move(promise), std::move(task));
    detach_on_series(std::move(t), series);
    return future;
}

/**
 * @brief Create coke::Future<T> from coke::Task<T> on new series created by
 *        creater, the task will be started immediately.
 */
template<Cokeable T, typename SeriesCreaterType>
Future<T> create_future(Task<T> &&task, SeriesCreaterType &&creater)
{
    Promise<T> promise;
    Future<T> future = promise.get_future();

    Task<> t = detail::detach_task(std::move(promise), std::move(task));
    detach_on_new_series(std::move(t), creater);
    return future;
}

/**
 * @brief Create coke::Future<T> from coke::Task<T> on new series created by
 *        default series creater, the task will be started immediately.
 */
template<Cokeable T>
Future<T> create_future(Task<T> &&task)
{
    return create_future(std::move(task), get_series_creater());
}

/**
 * @brief Wait until at least `n` futures is completed.
 * @pre No callback has been set for any future.
 * @return coke::Task<void> that should be co_await immediately.
 */
template<Cokeable T>
Task<void> wait_futures(std::vector<Future<T>> &futs, std::size_t n)
{
    if (n == 0)
        co_return;
    else if (n > futs.size())
        n = futs.size();

    detail::FutureWaitHelper helper(n);

    for (Future<T> &fut : futs) {
        fut.set_callback([&helper](int) { helper.count_down(); });
    }

    co_await helper.wait();

    for (Future<T> &fut : futs) {
        fut.remove_callback();
    }
}

/**
 * @brief Wait until at least `n` futures is completed, or `nsec` timeout.
 * @pre No callback has been set for any future.
 * @return coke::Task<int> that should be co_await immediately.
 * @retval coke::TOP_SUCCESS if at least `n` futures is completed.
 * @retval coke::TOP_TIMEOUT if `nsec` timeout.
 */
template<Cokeable T>
Task<int> wait_futures_for(std::vector<Future<T>> &futs, std::size_t n,
                           NanoSec nsec)
{
    if (n == 0)
        co_return TOP_SUCCESS;
    else if (n > futs.size())
        n = futs.size();

    detail::FutureWaitHelper helper(n);
    int ret;

    for (Future<T> &fut : futs) {
        fut.set_callback([&helper](int) { helper.count_down(); });
    }

    ret = co_await helper.wait_for(nsec);

    for (Future<T> &fut : futs) {
        fut.remove_callback();
    }

    if (ret == LATCH_SUCCESS)
        co_return TOP_SUCCESS;
    else
        co_return TOP_TIMEOUT;
}

} // namespace coke

#endif // COKE_FUTURE_H
