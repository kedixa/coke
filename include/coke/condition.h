/**
 * Copyright 2024-2025 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_CONDITION_H
#define COKE_CONDITION_H

#include "coke/detail/condition_impl.h"

namespace coke {

class Condition {
public:
    /**
     * @brief Create a Condition.
     */
    Condition() noexcept : wait_cnt(0) {}

    /**
     * @brief Condition is neither copyable nor movable.
     */
    Condition(const Condition &) = delete;
    Condition(Condition &&) = delete;

    Condition &operator=(const Condition &) = delete;
    Condition &operator=(Condition &&) = delete;

    ~Condition() = default;

    /**
     * @brief Blocks the current coroutine until the condition is awakened.
     *
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     *         See wait_for but ignore coke::TOP_TIMEOUT.
     */
    Task<int> wait(std::unique_lock<std::mutex> &lock)
    {
        return detail::cv_wait_impl(lock, get_addr(), detail::TimedWaitHelper{},
                                    &wait_cnt);
    }

    /**
     * @brief Blocks the current coroutine until the condition is awakened and
     *        pred() returns true.
     *
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     *         See wait_for but ignore coke::TOP_TIMEOUT.
     */
    Task<int> wait(std::unique_lock<std::mutex> &lock,
                   std::function<bool()> pred)
    {
        return detail::cv_wait_impl(lock, get_addr(), detail::TimedWaitHelper{},
                                    std::move(pred), &wait_cnt);
    }

    /**
     * @brief Blocks the current coroutine until the condition is awakened, or
     *        `nsec` timeout.
     *
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     * @retval coke::TOP_SUCCESS If awakened before timeout.
     * @retval coke::TOP_TIMEOUT If wait timeout.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
     */
    Task<int> wait_for(std::unique_lock<std::mutex> &lock, NanoSec nsec)
    {
        return detail::cv_wait_impl(lock, get_addr(),
                                    detail::TimedWaitHelper{nsec}, &wait_cnt);
    }

    /**
     * @brief Blocks the current coroutine until the condition is (awakened and
     *        pred returns true) or `nsec` timeout.
     *
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     *         See wait_for(lock, nsec), but coke::TOP_SUCCESS is returned
     *         only if `pred()` returns true.
     */
    Task<int> wait_for(std::unique_lock<std::mutex> &lock, NanoSec nsec,
                       std::function<bool()> pred)
    {
        return detail::cv_wait_impl(lock, get_addr(),
                                    detail::TimedWaitHelper{nsec},
                                    std::move(pred), &wait_cnt);
    }

    /**
     * @brief If any coroutines are waiting on *this, calling notify_one
     *        unblocks one of them.
     */
    void notify_one() { notify(1); }

    /**
     * @brief If any coroutines are waiting on *this, calling notify
     *        unblocks n of them.
     *
     * @param n The number of coroutines waiting on *this to wakeup, if less
     *        than n, wakeup all.
     */
    void notify(std::size_t n) { detail::cv_notify(get_addr(), n); }

    /**
     * @brief If any coroutines are waiting on *this, calling notify_all
     *        unblocks all of them.
     */
    void notify_all() { notify(std::size_t(-1)); }

protected:
    const void *get_addr() const noexcept { return (const char *)this + 1; }

private:
    // This count is not necessary, it is for debugging convenience, and make
    // *this has unique address.
    int wait_cnt;
};

} // namespace coke

#endif // COKE_CONDITION_H
