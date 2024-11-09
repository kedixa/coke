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

#ifndef COKE_LATCH_H
#define COKE_LATCH_H

#include <latch>

#include "coke/detail/mutex_table.h"
#include "coke/sleep.h"

namespace coke {

using LatchAwaiter = SleepAwaiter;

/**
 * @brief The return value of LatchAwaiter does not conform to normal semantics,
 *        define constant for it to distinguish from SleepAwaiter.
*/
constexpr int LATCH_SUCCESS = SLEEP_CANCELED;
constexpr int LATCH_TIMEOUT = SLEEP_SUCCESS;

class Latch final {
public:
    /**
     * @brief Create a latch that can be counted EXACTLY `n` times.
     * @param n Number to be counted and should >= 0.
    */
    explicit Latch(long n) noexcept
        : mtx(detail::get_mutex(get_addr())), expected(n)
    { }

    ~Latch() { }

    /**
     * @brief coke::Latch is neither copyable nor movable.
    */
    Latch(const Latch &) = delete;
    Latch &operator= (const Latch &) = delete;

    /**
     * @brief Return whether the internal counter has reached zero.
    */
    bool try_wait() const {
        std::lock_guard<std::mutex> lg(this->mtx);
        return this->expected >= 0;
    }

    /**
     * @brief Wait for the Latch to be counted to zero.
     * @return An awaiter that should be awaited immediately.
     * @retval coke::LATCH_SUCCESS.
     */
    LatchAwaiter wait() {
        return wait_impl(detail::TimedWaitHelper{});
    }

    /**
     * @brief Wait for the Latch to be counted to zero, or timeout.
     * @return An awaiter that should be awaited immediately.
     * @retval coke::LATCH_SUCCESS if Latch is counted to zero.
     * @retval coke::LATCH_TIMEOUT if `nsec` timeout.
     */
    LatchAwaiter wait_for(NanoSec nsec) {
        return wait_impl(detail::TimedWaitHelper{nsec});
    }

    /**
     * @brief Count by n and wait for the Latch to be counted to zero.
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
     *
     * @param n Count the Latch by n. n should >= 1.
     * @return An awaiter needs to be awaited immediately.
     */
    LatchAwaiter arrive_and_wait(long n = 1) {
        return create_awaiter(n);
    }

    /**
     * @brief Count down the Latch without wait. n should >= 1.
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
     *
     * @param n Count the Latch by n. n should >= 1.
    */
    void count_down(long n = 1);

private:
    const void *get_addr() const noexcept {
        return (const char *)this + 1;
    }

    LatchAwaiter wait_impl(detail::TimedWaitHelper helper);

    LatchAwaiter create_awaiter(long n);

private:
    std::mutex &mtx;
    long expected;
};

class SyncLatch final {
public:
    explicit SyncLatch(long n) : lt(n) { }

    void count_down(long n = 1) { lt.count_down(n); }

    void wait() const;

private:
    std::latch lt;
};

} // namespace coke

#endif // COKE_LATCH_H
