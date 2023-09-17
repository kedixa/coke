#ifndef COKE_FUTURE_H
#define COKE_FUTURE_H

#include <atomic>
#include <optional>
#include <memory>
#include <mutex>
#include <chrono>

#include "coke/detail/task.h"
#include "coke/sleep.h"
#include "coke/global.h"

namespace coke {

constexpr int FUTURE_STATE_INVALID  = -1;
constexpr int FUTURE_STATE_READY    = 0;
constexpr int FUTURE_STATE_TIMEOUT  = 1;
constexpr int FUTURE_STATE_ABORTED  = 2;

template<typename Res>
concept FutureResType = std::copyable<Res> || std::is_void_v<Res>;

template<FutureResType Res>
class Future;

template<FutureResType Res>
class Promise;

class FutureAwaiter : public BasicAwaiter<int> {
private:
    explicit FutureAwaiter(int ret);
    explicit FutureAwaiter(SubTask *task);

    template<FutureResType T>
    friend class Future;
};

namespace detail {

struct FutureStateBase {
public:
    FutureStateBase() : ready(false), counter(nullptr) { }

    /**
     * Check whether this future is already `ready`. The `ready` state is set by
     * this->wakeup(FUTURE_STATE_READY), `ready` state can only and must be set once.
    */
    bool is_ready() const {
        std::lock_guard<std::mutex> lg(this->mtx);
        return this->ready;
    }

    /**
     * Wake up the one who is waiting on this future, with state `state`.
     * The `state` is an integer named FUTURE_STATE_* on the above. The awakened
     * user needs to check the return value and do the corresponding operation.
    */
    void wakeup(int state);

    /**
     * Try to create wait task, if this future is ready or invalid, the function
     * return nullptr and we should wake up the caller with `state` immediately.
    */
    SubTask *create_wait_task(int &state);

private:
    bool ready;
    SubTask *counter;
    mutable std::mutex mtx;
};

template<typename T>
class FutureState : public FutureStateBase {
public:
    void set_value(const T &value) { opt.emplace(value); }
    void set_value(T &&value) { opt.emplace(std::move(value)); }
    T get_value() { return std::move(opt.value()); }

private:
    std::optional<T> opt;
};

template<>
class FutureState<void> : public FutureStateBase { };

} // namespace detail

template<FutureResType Res>
class Future {
    using state_t = detail::FutureState<Res>;
    using state_ptr_t = std::shared_ptr<state_t>;

public:
    Future() = default;
    Future(Future &&) = default;
    Future &operator=(Future &&) = default;
    ~Future() = default;

    Future(const Future &) = delete;
    Future &operator=(const Future &) = delete;

    /**
     * Return whether the state of this Future is valid.
    */
    bool valid() const { return state; }

    /**
     * Return whether the state of this Future is ready.
    */
    bool ready() const { return state && state->is_ready(); }

    /**
     * Blocks until the result becomes available.
     *
     * If `valid()` is false before call wait/wait_for/wait_until, the caller
     * will always get `FUTURE_STATE_INVALID`.
     *
     * Suppose `valid()` is true, and
     * 1. after co_await `wait`, caller always get `FUTURE_STATE_READY`.
     * 2. after co_await `wait_for/wait_until`, caller get
     *    `FUTURE_STATE_READY` if state is ready before timeout
     *    `FUTURE_STATE_TIMEOUT` if timeout before state is ready
     *    `FUTURE_STATE_ABORTED` if timer is aborted by process exit
     *
     *    It is the better to ensure that all coroutines finish before
     *    the process exits.
    */
    FutureAwaiter wait();

    /**
     * Blocks until the result becomes available or timeout.
     * See `wait`.
    */
    FutureAwaiter wait_for(std::chrono::nanoseconds ns);

    /**
     * Blocks until the result becomes available or timeout.
     * See `wait`.
    */
    template<typename Clock, typename Duration>
        requires std::chrono::is_clock_v<Clock>
    FutureAwaiter wait_until(std::chrono::time_point<Clock, Duration> abs_tp);

    /**
     * Get the value set by coke::Promise, `Future::get` can only be called once
     * after any of wait/wait_for/wair_until returns FUTURE_STATE_READY.
    */
    Res get() {
        if constexpr (!std::is_void_v<Res>)
            return state->get_value();
    }

private:
    Future(state_ptr_t ptr) : state(ptr) { }

    friend Promise<Res>;

    static coke::Task<> wait_helper(std::chrono::nanoseconds ns, state_ptr_t state) {
        int ret = co_await coke::sleep(ns);
        int s = (ret == STATE_SUCCESS) ? FUTURE_STATE_TIMEOUT : FUTURE_STATE_ABORTED;

        // If coke::sleep is aborted, which means this process is terminating,
        // then future's wait returns FUTURE_STATE_ABORTED and do not call wait any more
        state->wakeup(s);
    }

private:
    state_ptr_t state;
};

template<FutureResType Res>
class Promise {
    using state_t = detail::FutureState<Res>;
    using state_ptr_t = std::shared_ptr<state_t>;

public:
    Promise() { state = std::make_shared<state_t>(); }
    ~Promise() = default;

    Promise(Promise &&) = default;
    Promise &operator=(Promise &&) = default;

    Promise(const Promise &) = delete;
    Promise &operator=(const Promise &)  = delete;

    Future<Res> get_future() { return Future<Res>(state); }

    void set_value(const Res &value);
    void set_value(Res &&value);

private:
    state_ptr_t state;
};

template<>
class Promise<void> {
    using state_t = detail::FutureState<void>;
    using state_ptr_t = std::shared_ptr<state_t>;

public:
    Promise() { state = std::make_shared<state_t>(); }
    ~Promise() = default;

    Promise(Promise &&) = default;
    Promise &operator=(Promise &&) = default;

    Promise(const Promise &) = delete;
    Promise &operator=(const Promise &)  = delete;

    Future<void> get_future() { return Future<void>(state); }

    void set_value() {
        if (state)
            state->wakeup(FUTURE_STATE_READY);
    }

private:
    state_ptr_t state;
};

// Implementation

template<FutureResType Res>
FutureAwaiter Future<Res>::wait() {
    int s = FUTURE_STATE_INVALID;

    if (state) {
        SubTask *task = state->create_wait_task(s);
        if (task)
            return FutureAwaiter(task);
    }

    return FutureAwaiter(s);
}

template<FutureResType Res>
FutureAwaiter Future<Res>::wait_for(std::chrono::nanoseconds ns) {
    int s = FUTURE_STATE_INVALID;
    if (state) {
        SubTask *task = state->create_wait_task(s);
        if (task) {
            wait_helper(ns, state).start();
            return FutureAwaiter(task);
        }
    }

    return FutureAwaiter(s);
}

template<FutureResType Res>
template<typename Clock, typename Duration>
    requires std::chrono::is_clock_v<Clock>
FutureAwaiter Future<Res>::wait_until(std::chrono::time_point<Clock, Duration> abs_tp) {
    auto dur = abs_tp - Clock::now();
    return wait_for(dur);
}

template<FutureResType Res>
void Promise<Res>::set_value(const Res &value) {
    if (state) {
        state->set_value(value);
        state->wakeup(FUTURE_STATE_READY);
    }
}

template<FutureResType Res>
void Promise<Res>::set_value(Res &&value) {
    if (state) {
        state->set_value(std::move(value));
        state->wakeup(FUTURE_STATE_READY);
    }
}

} // namespace coke

#endif // COKE_FUTURE_H
