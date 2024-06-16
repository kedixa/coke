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

#include <vector>
#include <mutex>
#include <latch>

#include "coke/detail/awaiter_base.h"

namespace coke {

class [[nodiscard]] LatchAwaiter : public BasicAwaiter<void> {
private:
    LatchAwaiter() { }
    explicit LatchAwaiter(SubTask *task);

    friend class Latch;
};

class Latch final {
public:
    /**
     * @brief Create a latch that can be counted EXACTLY `n` times.
     * @param n Number to be counted and should >= 0.
    */
    explicit Latch(long n) : expected(n) { }
    ~Latch() { }

    /**
     * @brief coke::Latch is not copyable.
    */
    Latch(const Latch &) = delete;

    /**
     * @brief See Latch::wait.
     * 
     *  Latch l(...);
     * 
     *  co_await l; is same as co_await l.wait();
    */
    LatchAwaiter operator co_await() {
        return wait();
    }

    /**
     * @brief Wait for the Latch to be counted to zero.
     * @return An awaiter needs to be awaited immediately.
     */
    LatchAwaiter wait() noexcept {
        return create_awaiter(0);
    }

    /**
     * @brief Count by n and wait for the Latch to be counted to zero.
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
     *
     * @param n Count the Latch by n. n should >= 1.
     * @return An awaiter needs to be awaited immediately.
     */
    LatchAwaiter arrive_and_wait(long n = 1) noexcept {
        return create_awaiter(n);
    }

    /**
     * @brief Count down the Latch without wait. n should >= 1.
     * The sum of all n's in `arrive_and_wait` and `count_down` MUST equal
     * to the `n` passed into Latch's constructor.
     *
     * @param n Count the Latch by n. n should >= 1.
    */
    void count_down(long n = 1) noexcept;

private:
    LatchAwaiter create_awaiter(long n) noexcept;

    static void wake_up(std::vector<SubTask *> tasks) noexcept;

private:
    std::mutex mtx;
    long expected;
    std::vector<SubTask *> tasks;
};

class SyncLatch final {
public:
    explicit SyncLatch(long n) : lt(n) { }

    void count_down(long n = 1) { lt.count_down(n); }

    void wait() const noexcept;

private:
    std::latch lt;
};

} // namespace coke

#endif // COKE_LATCH_H
