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

    bool valid() const { return state; }
    bool ready() const { return state && state->is_ready(); }

    FutureAwaiter wait();
    FutureAwaiter wait_for(std::chrono::nanoseconds ns);

    template<typename Clock, typename Duration>
        requires std::chrono::is_clock_v<Clock>
    FutureAwaiter wait_until(std::chrono::time_point<Clock, Duration> abs_tp);

    Res get() requires (!std::is_void_v<Res>) { return state->get_value(); }
    void get() requires (std::is_void_v<Res>) { }

private:
    Future(state_ptr_t ptr) : state(ptr) { }

    friend Promise<Res>;

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

    void set_value() requires std::is_void_v<Res>;
    void set_value(const Res &value) requires (!std::is_void_v<Res>);
    void set_value(Res &&value) requires (!std::is_void_v<Res>);

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
    auto w = [](std::chrono::nanoseconds ns, state_ptr_t state) -> coke::Task<> {
        int ret = co_await coke::sleep(ns);
        int s = (ret == STATE_SUCCESS) ? FUTURE_STATE_TIMEOUT : FUTURE_STATE_ABORTED;

        // If coke::sleep is aborted, which means this process is terminating,
        // then future's wait returns FUTURE_STATE_ABORTED and do not call wait any more
        state->wakeup(s);
    };

    int s = FUTURE_STATE_INVALID;
    if (state) {
        SubTask *task = state->create_wait_task(s);
        if (task) {
            w(ns, state).start();
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
void Promise<Res>::set_value() requires std::is_void_v<Res> {
    if (state)
        state->wakeup(FUTURE_STATE_READY);
}

template<FutureResType Res>
void Promise<Res>::set_value(const Res &value) requires (!std::is_void_v<Res>) {
    if (state) {
        state->set_value(value);
        state->wakeup(FUTURE_STATE_READY);
    }
}

template<FutureResType Res>
void Promise<Res>::set_value(Res &&value) requires (!std::is_void_v<Res>) {
    if (state) {
        state->set_value(std::move(value));
        state->wakeup(FUTURE_STATE_READY);
    }
}

} // namespace coke

#endif // COKE_FUTURE_H
