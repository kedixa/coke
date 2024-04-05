#ifndef COKE_FUTURE_H
#define COKE_FUTURE_H

#include <atomic>
#include <optional>
#include <memory>
#include <mutex>
#include <chrono>

#include "coke/detail/task.h"
#include "coke/detail/mutex_table.h"
#include "coke/sleep.h"
#include "coke/global.h"

namespace coke {

constexpr int FUTURE_STATE_BROKEN   = -1;
constexpr int FUTURE_STATE_READY    = 0;
constexpr int FUTURE_STATE_TIMEOUT  = 1;
constexpr int FUTURE_STATE_ABORTED  = 2;

template<Cokeable Res>
class Future;

template<Cokeable Res>
class Promise;

namespace detail {

struct FutureStateBase {
public:
    FutureStateBase()
        : is_ready(false), is_broken(false),
          uid(INVALID_UNIQUE_ID), mtx(get_mutex(this))
    { }

    bool ready() const {
        std::lock_guard<std::mutex> lg(mtx);
        return is_ready;
    }

    bool broken() const {
        std::lock_guard<std::mutex> lg(mtx);
        return is_broken;
    }

    void set_broken() {
        std::lock_guard<std::mutex> lg(mtx);
        is_broken = true;
        wakeup_unlocked();
    }

    Task<int> wait() {
        return wait_impl(TimedWaitHelper{});
    }

    Task<int> wait_for(const NanoSec &nsec) {
        return wait_impl(TimedWaitHelper{nsec});
    }

    void wakeup_unlocked() {
        if (this->uid != INVALID_UNIQUE_ID)
            cancel_sleep_by_id(uid);
    }

protected:
    Task<int> wait_impl(TimedWaitHelper helper) {
        constexpr NanoSec zero(0);

        std::unique_lock<std::mutex> lk(mtx);
        if (is_broken)
            co_return FUTURE_STATE_BROKEN;

        if (is_ready)
            co_return FUTURE_STATE_READY;

        if (uid == INVALID_UNIQUE_ID)
            uid = get_unique_id();

        auto dur = helper.time_left();
        if (dur <= zero)
            co_return  FUTURE_STATE_TIMEOUT;

        SleepAwaiter s = helper.infinite()
                         ? sleep(uid, InfiniteDuration{})
                         : sleep(uid, dur);
        lk.unlock();
        int ret = co_await s;
        lk.lock();

        if (is_broken)
            co_return FUTURE_STATE_BROKEN;
        else if (is_ready)
            co_return FUTURE_STATE_READY;
        else if (ret == 0)
            co_return FUTURE_STATE_TIMEOUT;
        else
            co_return ret;
    }

protected:
    bool is_ready;
    bool is_broken;
    uint64_t uid;
    std::mutex &mtx;
};

template<typename T>
class FutureState : public FutureStateBase {
public:
    void set_value(const T &value) {
        std::lock_guard<std::mutex> lg(mtx);
        opt.emplace(value);
        this->is_ready = true;
        this->wakeup_unlocked();
    }

    void set_value(T &&value) {
        std::lock_guard<std::mutex> lg(mtx);
        opt.emplace(std::move(value));
        this->is_ready = true;
        this->wakeup_unlocked();
    }

    T &get() { return opt.value(); }

private:
    std::optional<T> opt;
};

template<>
class FutureState<void> : public FutureStateBase {
public:
    void set_value() {
        std::lock_guard<std::mutex> lg(mtx);
        this->is_ready = true;
        this->wakeup_unlocked();
    }
};

} // namespace detail


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
    bool valid() const { return (bool)state; }

    /**
     * @pre valid() returns true.
     *
     * @brief Return whether the state of this Future is ready.
    */
    bool ready() const { return state->ready(); }

    /**
     * @pre valid() returns true.
     *
     * @brief Return whether the promise destroyed without set value.
    */
    bool broken() const { return state->broken(); }

    /**
     * @pre valid() returns true.
     *
     * @brief Blocks until ready() or broken().
     *
     * @details
     * After co_await you may get
     *    `FUTURE_STATE_BROKEN` if promise destroyed without set value.
     *    `FUTURE_STATE_READY` if state is ready or broken before timeout.
     *    `FUTURE_STATE_TIMEOUT` if timeout before state is ready or broken.
     *    `FUTURE_STATE_ABORTED` if timer is aborted by process exit.
     *
     *    It is the better to ensure that all coroutines finish before
     *    the process exits.
    */
    [[nodiscard]]
    Task<int> wait() { return state->wait(); }

    /**
     * @pre valid() returns true.
     *
     * @brief Blocks until ready() or broken() or timeout.
     *
     * See Future::wait() for more infomation.
     */
    [[nodiscard]]
    Task<int> wait_for(const NanoSec &nsec) {
        return state->wait_for(nsec);
    }

    /**
     * @pre ready() returns true.
     *
     * @brief Gets the value set by the Promise associated with the Future. This
     *        function can only be called at most once.
     *
     * @post The saved value is in an undefined state.
    */
    Res get() {
        if constexpr (!std::is_void_v<Res>)
            return std::move(state->get());
    }

private:
    /**
     * @brief Future can only be created by Promise.
    */
    Future(std::shared_ptr<State> ptr) : state(ptr) { }

    friend Promise<Res>;

private:
    std::shared_ptr<State> state;
}; // class Future

template<Cokeable Res>
class Promise {
    using State = detail::FutureState<Res>;

public:
    Promise() { state = std::make_shared<State>(); }
    ~Promise() {
        if (state && !state->ready())
            state->set_broken();
    }

    Promise(Promise &&) = default;
    Promise &operator=(Promise &&) = default;

    Promise(const Promise &) = delete;
    Promise &operator=(const Promise &)  = delete;

    /**
     * @brief Get the Future associated with this Promise. This function can
     *        only be called at most once.
    */
    Future<Res> get_future() { return Future<Res>(state); }

    /**
     * @brief Set value and wake up Future. These two `set_value` can only be
     *        called at most once.
    */
    void set_value(const Res &value) { state->set_value(value); }

    /**
     * @brief Set value and wake up Future. These two `set_value` can only be
     *        called at most once.
    */
    void set_value(Res &&value) { state->set_value(std::move(value)); }

private:
    std::shared_ptr<State> state;
};

template<>
class Promise<void> {
    using State = detail::FutureState<void>;

public:
    Promise() { state = std::make_shared<State>(); }
    ~Promise() {
        if (state && !state->ready())
            state->set_broken();
    }

    Promise(Promise &&) = default;
    Promise &operator=(Promise &&) = default;

    Promise(const Promise &) = delete;
    Promise &operator=(const Promise &)  = delete;

    /**
     * @brief Get the Future associated with this Promise. This function can
     *        only be called at most once.
    */
    Future<void> get_future() { return Future<void>(state); }

    /**
     * @brief Set value and wake up Future. This function can only be called at
     *        most once.
    */
    void set_value() { state->set_value(); }

private:
    std::shared_ptr<State> state;
};

namespace detail {

template<Cokeable T>
Task<void> detach_task(coke::Promise<T> promise, Task<T> task) {
    if constexpr (std::is_void_v<T>) {
        co_await task;
        promise.set_value();
    }
    else {
        promise.set_value(co_await task);
    }

    co_return;
}

} // namespace  detail


/**
 * @brief Create coke::Future from coke::Task, the task will be started
 *        immediately.
*/
template<Cokeable T>
Future<T> create_future(Task<T> &&task) {
    Promise<T> promise;
    Future<T> future = promise.get_future();

    detail::detach_task(std::move(promise), std::move(task)).start();
    return future;
}

} // namespace coke

#endif // COKE_FUTURE_H
