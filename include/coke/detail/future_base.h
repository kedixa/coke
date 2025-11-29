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

#ifndef COKE_DETAIL_FUTURE_BASE_H
#define COKE_DETAIL_FUTURE_BASE_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "coke/detail/exception_config.h"
#include "coke/latch.h"
#include "coke/sleep.h"
#include "coke/task.h"

namespace coke {

constexpr int FUTURE_STATE_READY = 0;
constexpr int FUTURE_STATE_TIMEOUT = 1;
constexpr int FUTURE_STATE_ABORTED = 2;
constexpr int FUTURE_STATE_BROKEN = 3;
constexpr int FUTURE_STATE_EXCEPTION = 4;
constexpr int FUTURE_STATE_NOTSET = 5;

template<Cokeable Res>
class Future;

template<Cokeable Res>
class Promise;

namespace detail {

struct FutureStateBase {
    constexpr static auto acquire = std::memory_order_acquire;
    constexpr static auto release = std::memory_order_release;

public:
    FutureStateBase() noexcept : canceled(false), state(FUTURE_STATE_NOTSET) {}

    int get_state() const noexcept { return state.load(acquire); }

    bool set_broken()
    {
        return set_once([](bool *flag) { *flag = true; }, FUTURE_STATE_BROKEN);
    }

    bool set_exception(const std::exception_ptr &eptr)
    {
        return set_once(
            [&, this](bool *flag) {
                this->eptr = eptr;
                *flag = true;
            },
            FUTURE_STATE_EXCEPTION);
    }

    std::exception_ptr get_exception() { return eptr; }

    void raise_exception()
    {
        if (eptr)
            std::rethrow_exception(eptr);
    }

    Task<int> wait() { return wait_impl(TimedWaitHelper{}); }

    Task<int> wait_for(NanoSec nsec)
    {
        return wait_impl(TimedWaitHelper{nsec});
    }

    void set_callback(std::function<void(int)> &&cb)
    {
        std::lock_guard<std::mutex> lg(mtx);

        int st = get_state();
        if (st != FUTURE_STATE_NOTSET)
            cb(st);
        else
            callback = std::move(cb);
    }

    void remove_callback()
    {
        std::lock_guard<std::mutex> lg(mtx);
        callback = nullptr;
    }

    void set_canceled() { canceled.store(true, release); }

    bool is_canceled() const noexcept { return canceled.load(acquire); }

protected:
    template<typename Callable>
    bool set_once(Callable &&func, int new_state)
    {
        bool set_succ = false;
        std::call_once(once_flag, std::forward<Callable>(func), &set_succ);

        if (set_succ) {
            state.store(new_state, release);
            this->wakeup();
        }

        return set_succ;
    }

    void wakeup()
    {
        std::lock_guard<std::mutex> lg(mtx);

        if (callback) {
            int st = get_state();
            callback(st);
            callback = nullptr;
        }

        cancel_sleep_by_addr(get_addr());
    }

    Task<int> wait_impl(TimedWaitHelper helper)
    {
        int st = get_state();
        if (st != FUTURE_STATE_NOTSET)
            co_return st;

        if (helper.timeout())
            co_return FUTURE_STATE_TIMEOUT;

        std::unique_lock<std::mutex> lk(mtx);

        auto s = sleep(get_addr(), helper);
        st = get_state();
        if (st != FUTURE_STATE_NOTSET)
            co_return st;

        lk.unlock();
        int ret = co_await std::move(s);
        lk.lock();

        st = get_state();
        if (st == FUTURE_STATE_NOTSET) {
            if (ret == 0)
                co_return FUTURE_STATE_TIMEOUT;
            else
                co_return ret;
        }

        co_return st;
    }

    const void *get_addr() const noexcept { return (const char *)this + 1; }

protected:
    std::once_flag once_flag;
    std::atomic<bool> canceled;
    std::atomic<int> state;

    std::mutex mtx;
    std::function<void(int)> callback;
    std::exception_ptr eptr;
};

template<typename T>
class FutureState : public FutureStateBase {
public:
    bool set_value(const T &value)
    {
        return set_once(
            [&, this](bool *flag) {
                this->opt.emplace(value);
                *flag = true;
            },
            FUTURE_STATE_READY);
    }

    bool set_value(T &&value)
    {
        return set_once(
            [&, this](bool *flag) {
                this->opt.emplace(std::move(value));
                *flag = true;
            },
            FUTURE_STATE_READY);
    }

    T &get()
    {
        raise_exception();
        return opt.value();
    }

private:
    std::optional<T> opt;
};

template<>
class FutureState<void> : public FutureStateBase {
public:
    bool set_value()
    {
        return set_once([](bool *flag) { *flag = true; }, FUTURE_STATE_READY);
    }

    void get() { raise_exception(); }
};

template<Cokeable T>
Task<void> detach_task(coke::Promise<T> promise, Task<T> task)
{
    coke_try
    {
        if constexpr (std::is_void_v<T>) {
            co_await std::move(task);
            promise.set_value();
        }
        else {
            promise.set_value(co_await std::move(task));
        }
    }
    coke_catch(...)
    {
        promise.set_exception(std::current_exception());
    }

    co_return;
}

class FutureWaitHelper {
public:
    FutureWaitHelper(std::size_t n) noexcept : lt(1), x(0), n(n) {}

    void count_down()
    {
        if (x.fetch_add(1, std::memory_order_relaxed) + 1 == n)
            lt.count_down(1);
    }

    auto wait() { return lt.wait(); }

    auto wait_for(NanoSec nsec) { return lt.wait_for(nsec); }

private:
    Latch lt;
    std::atomic<std::size_t> x;
    std::size_t n;
};

} // namespace detail

} // namespace coke

#endif // COKE_DETAIL_FUTURE_BASE_H
