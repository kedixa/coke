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

#ifndef COKE_STOP_TOKEN_H
#define COKE_STOP_TOKEN_H

#include <atomic>

#include "coke/detail/mutex_table.h"
#include "coke/task.h"
#include "coke/sleep.h"

namespace coke {

class StopToken final {
public:
    /**
     * @brief Create a StopToken.
     *
     * @param cnt When set_finished() is called exactly `cnt` times, the state
     *        of stop token become finished.
    */
    explicit StopToken(std::size_t cnt = 1) noexcept
        : mtx(detail::get_mutex(this)), n(cnt), should_stop(false)
    { }

    StopToken(const StopToken &) = delete;
    StopToken &operator= (const StopToken &) = delete;

    ~StopToken() = default;

    /**
     * @brief Reset the inner state, prepare for next use.
     * @pre The previous process has ended normally.
    */
    void reset(std::size_t cnt) noexcept {
        n = cnt;
        should_stop.store(false, std::memory_order_relaxed);
    }

    /**
     * @brief Request the coroutine working with this stop token to stop.
     * @post stop_requested() will returns true.
    */
    void request_stop() {
        std::lock_guard<std::mutex> lg(mtx);
        should_stop.store(true, std::memory_order_release);
        cancel_sleep_by_addr(get_stop_addr());
    }

    /**
     * @brief Return whether stop is requested.
    */
    bool stop_requested() const noexcept {
        return should_stop.load(std::memory_order_acquire);
    }

    /**
     * @brief Decrease `n` by one, when it reches to zero, wake up coroutine
     *        that is waiting for finish.
    */
    void set_finished() {
        std::lock_guard<std::mutex> lg(mtx);
        if (n > 0 && --n == 0)
            cancel_sleep_by_addr(get_finish_addr());
    }

    /**
     * @brief Return whether set_finished() has been called `n` times.
    */
    bool finished() const {
        std::lock_guard<std::mutex> lg(mtx);
        return n == 0;
    }

    /**
     * @brief Wait until finish.
    */
    Task<bool> wait_finish() {
        return wait_finish_impl(detail::TimedWaitHelper{});
    }

    /**
     * @brief Wait until finish or timeout.
     * @return Whether it is finished.
    */
    Task<bool> wait_finish_for(NanoSec nsec) {
        return wait_finish_impl(detail::TimedWaitHelper{nsec});
    }

    /**
     * @brief Wait until stop requested or timeout.
     * @return Whether stop is requested.
    */
    Task<bool> wait_stop_for(NanoSec nsec) {
        return wait_stop_impl(detail::TimedWaitHelper{nsec});
    }

    struct FinishGuard {
        explicit FinishGuard(StopToken *sptr = nullptr) noexcept
            : ptr(sptr)
        { }

        FinishGuard(const FinishGuard &) = delete;
        FinishGuard &operator= (const FinishGuard &) = delete;

        void reset(StopToken *sptr) noexcept { ptr = sptr; }
        void release() noexcept { ptr = nullptr; }

        ~FinishGuard() {
            if (ptr)
                ptr->set_finished();
        }

    private:
        StopToken *ptr;
    };

private:
    Task<bool> wait_finish_impl(detail::TimedWaitHelper helper);

    Task<bool> wait_stop_impl(detail::TimedWaitHelper helper);

    const void *get_stop_addr() const noexcept {
        return (const char *)this + 1;
    }

    const void *get_finish_addr() const noexcept {
        return (const char *)this + 2;
    }

private:
    std::mutex &mtx;
    std::size_t n;
    std::atomic<bool> should_stop;
};

} // namespace coke

#endif // COKE_STOP_TOKEN_H
