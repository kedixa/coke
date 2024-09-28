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

#ifndef COKE_WAIT_GROUP_H
#define COKE_WAIT_GROUP_H

#include <atomic>

#include "coke/sleep.h"

namespace coke {

using WaitGroupAwaiter = SleepAwaiter;

/**
 * @brief The return value of WaitGroupAwaiter does not conform to normal
 *        semantics, define constant for it to distinguish from SleepAwaiter.
 */
constexpr int WAIT_GROUP_SUCCESS = SLEEP_CANCELED;

class WaitGroup final {
public:
    /**
     * @brief Create an empty WaitGroup.
     */
    WaitGroup() : count(0) { }

    /**
     * @brief WaitGroup is neither copyable nor movable.
    */
    WaitGroup(const WaitGroup &) = delete;
    WaitGroup &operator= (const WaitGroup &) = delete;

    ~WaitGroup() = default;

    /**
     * @brief Add `n` counts to this WaitGroup.
     * @param n Integer that must greater than 0.
     *
     * The number of calls to done must match the sum of n in add.
     */
    void add(long n) {
        count.fetch_add(n, std::memory_order_relaxed);
    }

    /**
     * @brief Notify that one count is complete.
     */
    void done() {
        if (count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            cancel_sleep_by_addr(get_addr());
        }
    }

    /**
     * @brief Wait for all counts to complete.
     */
    WaitGroupAwaiter wait() {
        SleepAwaiter a(SleepAwaiter::ImmediateTag{}, WAIT_GROUP_SUCCESS);
        auto c = load_count();

        if (c > 0) {
            a = sleep(get_addr(), inf_dur);

            if (load_count() <= 0)
                cancel_sleep_by_addr(get_addr());
        }

        return a;
    }

private:
    const void *get_addr() {
        return (const char *)this + 1;
    }

    long load_count() {
        return count.load(std::memory_order_acquire);
    }

private:
    std::atomic<long> count;
};

}

#endif // COKE_WAIT_GROUP_H
