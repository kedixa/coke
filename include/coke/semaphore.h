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

#ifndef COKE_SEMAPHORE_H
#define COKE_SEMAPHORE_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class TimedSemaphore {
public:
    using CountType = uint32_t;

    /**
     * @brief Create a TimedSemaphore, with initial count `n`.
    */
    explicit TimedSemaphore(CountType n)
        : count(n), waiting(0)
    { }

    /**
     * @brief TimedSemaphore is neither copyable nor movable.
    */
    TimedSemaphore(const TimedSemaphore &) = delete;
    TimedSemaphore &operator= (const TimedSemaphore &) = delete;

    ~TimedSemaphore() = default;

    /**
     * @brief Try to acquire a count.
     *
     * @pre Current coroutine doesn't owns all the counts.
     * @return Return true if acquire success, else false.
    */
    bool try_acquire() {
        std::lock_guard<std::mutex> lg(mtx);

        if (count > 0) {
            --count;
            return true;
        }

        return false;
    }

    /**
     * @brief Release `cnt` counts, and wake up those who are waiting.
    */
    void release(CountType cnt = 1) {
        std::lock_guard<std::mutex> lg(mtx);
        count += cnt;

        if (waiting > 0 && cnt > 0)
            cancel_sleep_by_addr(get_addr(), waiting > cnt ? cnt : waiting);
    }

    /**
     * @brief Acquire a count, block until success.
     *
     * @pre Current coroutine doesn't owns all the count.
     * @return See try_acquire_for but ignore coke::TOP_TIMEOUT.
    */
    Task<int> acquire() {
        return acquire_impl(detail::TimedWaitHelper{});
    }

    /**
     * @brief Acquire a count, block until success or `nsec` timeout.
     *
     * @pre Current coroutine doesn't owns all the count.
     * @param nsec Max time to block.
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     * @retval coke::TOP_SUCCESS If acquire success.
     * @retval coke::TOP_TIMEOUT If wait timeout.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    Task<int> try_acquire_for(NanoSec nsec) {
        return acquire_impl(detail::TimedWaitHelper(nsec));
    }

protected:
    /**
     * The `helper` variable must not be a reference to ensure that it exists
     * until the coroutine ends.
    */
    Task<int> acquire_impl(detail::TimedWaitHelper helper);

    const void *get_addr() const noexcept {
        return (const char *)this + 1;
    }

private:
    std::mutex mtx;
    CountType count;
    CountType waiting;
};

} // namespace coke

#endif // COKE_SEMAPHORE_H
