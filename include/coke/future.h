#ifndef COKE_FUTURE_H
#define COKE_FUTURE_H

#include <atomic>
#include <optional>
#include <memory>
#include <mutex>

#include "coke/detail/task.h"
#include "coke/detail/mutex_table.h"
#include "coke/sleep.h"

namespace coke {

constexpr int FUTURE_STATE_READY        = 0;
constexpr int FUTURE_STATE_TIMEOUT      = 1;
constexpr int FUTURE_STATE_ABORTED      = 2;
constexpr int FUTURE_STATE_BROKEN       = 3;
constexpr int FUTURE_STATE_EXCEPTION    = 4;
constexpr int FUTURE_STATE_NOTSET       = 5;

template<Cokeable Res>
class Future;

template<Cokeable Res>
class Promise;

namespace detail {

struct FutureStateBase {
    constexpr static auto acquire = std::memory_order_acquire;
    constexpr static auto release = std::memory_order_release;

public:
    FutureStateBase()
        : state(FUTURE_STATE_NOTSET), mtx(get_mutex(this)),
          uid(INVALID_UNIQUE_ID)
    { }

    int get_state() const { return state.load(acquire); }

    bool set_broken() {
        return set_once([](bool *flag) { *flag = true; },
                        FUTURE_STATE_BROKEN);
    }

    bool set_exception(const std::exception_ptr &eptr) {
        return set_once([&, this](bool *flag) {
            this->eptr = eptr;
            *flag = true;
        }, FUTURE_STATE_EXCEPTION);
    }

    std::exception_ptr get_exception() { return eptr; }

    void raise_exception() {
        if (eptr)
            std::rethrow_exception(eptr);
    }

    Task<int> wait() { return wait_impl(TimedWaitHelper{}); }

    Task<int> wait_for(const NanoSec &nsec) {
        return wait_impl(TimedWaitHelper{nsec});
    }

protected:
    template<typename Callable>
    bool set_once(Callable &&func, int new_state) {
        bool set_succ = false;
        std::call_once(once_flag, std::forward<Callable>(func), &set_succ);

        if (set_succ) {
            state.store(new_state, release);
            this->wakeup();
        }

        return set_succ;
    }

    void wakeup() {
        std::lock_guard<std::mutex> lg(mtx);
        if (uid != INVALID_UNIQUE_ID)
            cancel_sleep_by_id(uid);
    }

    Task<int> wait_impl(TimedWaitHelper helper) {
        constexpr NanoSec zero(0);

        int st = get_state();
        if (st != FUTURE_STATE_NOTSET)
            co_return st;

        std::unique_lock<std::mutex> lk(mtx);

        if (uid == INVALID_UNIQUE_ID)
            uid = get_unique_id();

        auto dur = helper.time_left();
        if (dur <= zero)
            co_return FUTURE_STATE_TIMEOUT;

        SleepAwaiter s = helper.infinite()
                         ? sleep(uid, InfiniteDuration{})
                         : sleep(uid, dur);

        st = get_state();
        if (st != FUTURE_STATE_NOTSET)
            co_return st;

        lk.unlock();
        int ret = co_await s;
        lk.lock();

        st = get_state();
        if (st == FUTURE_STATE_NOTSET)
            co_return ((ret == 0) ? FUTURE_STATE_TIMEOUT : ret);
        co_return st;
    }

protected:
    std::once_flag once_flag;
    std::atomic<int> state;

    std::mutex &mtx;
    uint64_t uid;
    std::exception_ptr eptr;
};

template<typename T>
class FutureState : public FutureStateBase {
public:
    bool set_value(const T &value) {
        return set_once([&, this](bool *flag) {
            this->opt.emplace(value);
            *flag = true;
        }, FUTURE_STATE_READY);
    }

    bool set_value(T &&value) {
        return set_once([&, this](bool *flag) {
            this->opt.emplace(std::move(value));
            *flag = true;
        }, FUTURE_STATE_READY);
    }

    T &get() {
        raise_exception();
        return opt.value();
    }

private:
    std::optional<T> opt;
};

template<>
class FutureState<void> : public FutureStateBase {
public:
    bool set_value() {
        return set_once([](bool *flag) {
            *flag = true;
        }, FUTURE_STATE_READY);
    }

    void get() { raise_exception(); }
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
    bool ready() const { return state->get_state() == FUTURE_STATE_READY; }

    /**
     * @pre valid() returns true.
     *
     * @brief Return whether the promise destroyed without set value.
    */
    bool broken() const { return state->get_state() == FUTURE_STATE_BROKEN; }

    /**
     * @pre valid() returns true.
     *
     * @brief Return whether there is an exception.
    */
    bool has_exception() const {
        return state->get_state() == FUTURE_STATE_EXCEPTION;
    }

    /**
     * @pre valid() returns true.
     *
     * @brief Blocks until ready, broken or exception.
     *
     * @details
     * After co_await you may get
     *    `FUTURE_STATE_BROKEN` if promise destroyed without set value.
     *    `FUTURE_STATE_READY` if state is ready.
     *    `FUTURE_STATE_EXCEPTION` if there is an exception.
     *    `FUTURE_STATE_TIMEOUT` if timeout before state is ready, broken or
     *    exception.
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
     * @brief Blocks until ready, broken, exception or timeout.
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
     * @brief Gets the value set by the Promise associated with the Future.
     *        If there is an exception set by Promise, it will be rethrow.
     *        This function can only be called at most once.
     *
     * @post The saved value is in an undefined state.
    */
    Res get() {
        if constexpr (!std::is_void_v<Res>)
            return std::move(state->get());
        else
            return state->get();
    }

    /**
     * @pre exception() returns true.
     *
     * @brief Get the exception  associated with the Future.
    */
    std::exception_ptr get_exception() { return state->get_exception(); }

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
        if (state && state->get_state() == FUTURE_STATE_NOTSET)
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
     * @brief Set value and wake up Future.
     *
     * @exception When copy constructor of Res throws.
     *
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
    */
    bool set_value(const Res &value) { return state->set_value(value); }

    /**
     * @brief Set value and wake up Future.
     *
     * @exception When move constructor of Res throws.
     *
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
    */
    bool set_value(Res &&value) { return state->set_value(std::move(value)); }

    /**
     * @brief Set exception and wake up Future.
     *
     * @exception When copy constructor of std::exception_ptr throws.
     *
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
    */
    bool set_exception(const std::exception_ptr &eptr) {
        return state->set_exception(eptr);
    }

private:
    std::shared_ptr<State> state;
};

template<>
class Promise<void> {
    using State = detail::FutureState<void>;

public:
    Promise() { state = std::make_shared<State>(); }
    ~Promise() {
        if (state && state->get_state() == FUTURE_STATE_NOTSET)
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
     * @brief Set value and wake up Future.
     *
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
    */
    bool set_value() { return state->set_value(); }

    /**
     * @brief Set exception and wake up Future.
     *
     * @exception When copy constructor of std::exception_ptr throws.
     *
     * @return true if set success, false when the state is already satisfied.
     *         Only one of ready, broken or exception can be set.
    */
    bool set_exception(const std::exception_ptr &eptr) {
        return state->set_exception(eptr);
    }

private:
    std::shared_ptr<State> state;
};

namespace detail {

template<Cokeable T>
Task<void> detach_task(coke::Promise<T> promise, Task<T> task) {
    try {
        if constexpr (std::is_void_v<T>) {
            co_await task;
            promise.set_value();
        }
        else {
            promise.set_value(co_await task);
        }
    }
    catch (...) {
        promise.set_exception(std::current_exception());
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
