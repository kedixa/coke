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

#ifndef COKE_MUTEX_H
#define COKE_MUTEX_H

#include "coke/semaphore.h"

namespace coke {

class TimedMutex {
public:
    /**
     * @brief Create a TimedMutex, the uid is automatically obtained from
     *        `coke::get_unique_id`.
    */
    TimedMutex() : sem(1) { }

    /**
     * @brief Create a TimedMutex with uid specified by user.
    */
    explicit TimedMutex(uint64_t uid) : sem(1, uid) { }

    /**
     * @brief TimedMutex is not copy or move constructible.
    */
    TimedMutex(const TimedMutex &) = delete;
    TimedMutex &operator= (const TimedMutex &) = delete;

    ~TimedMutex() = default;

    /**
     * @brief Get the unique id.
    */
    uint64_t get_uid() const { return sem.get_uid(); }

    /**
     * @brief Try to lock the mutex for exclusive ownership.
     *
     * @return Return true if lock success, else false.
     *
     * @pre Current coroutine doesn't owns the mutex.
    */
    bool try_lock() { return sem.try_acquire(); }

    /**
     * @brief Unlock the mutex.
     *
     * @pre Current coroutine must owns the mutex.
    */
    void unlock() { return sem.release(1); }

    /**
     * @brief Lock the mutex, block until success.
     *
     * @pre Current coroutine doesn't owns the mutex.
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     *         See try_lock_for but ignore coke::TOP_TIMEOUT.
    */
    Task<int> lock() { return sem.acquire(); }

    /**
     * @brief Lock the mutex, block until success or `nsec` timeout.
     *
     * @pre Current coroutine doesn't owns the mutex.
     * @param nsec Max time to block.
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     * @retval coke::TOP_SUCCESS If lock success.
     * @retval coke::TOP_TIMEOUT If `nsec` timeout.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    Task<int> try_lock_for(const NanoSec &nsec) {
        return sem.try_acquire_for(nsec);
    }

private:
    TimedSemaphore sem;
};

} // namespace coke

#endif // COKE_MUTEX_H
