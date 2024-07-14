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

#ifndef COKE_SHARED_MUTEX_H
#define COKE_SHARED_MUTEX_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class SharedTimedMutex {
    enum class State : uint8_t {
        Idle        = 0,
        Reading     = 1,
        Writing     = 2,
    };

public:
    using CountType = uint32_t;

    /**
     * @brief Create a SharedTimedMutex.
    */
    SharedTimedMutex()
        : read_doing(0), read_waiting(0), write_waiting(0), state(State::Idle)
    { }

    /**
     * @brief SharedTimedMutex is neither copyable nor movable.
    */
    SharedTimedMutex(const SharedTimedMutex &) = delete;
    SharedTimedMutex &operator= (const SharedTimedMutex &) = delete;

    ~SharedTimedMutex() = default;

    /**
     * @brief Try to lock the mutex for exclusive ownership.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @return Return true if lock success, else false.
    */
    bool try_lock() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Idle) {
            state = State::Writing;
            return true;
        }

        return false;
    }

    /**
     * @brief Try to lock the mutex for shared ownership.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @return Return true if lock success, else false.
    */
    bool try_lock_shared() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Idle || state == State::Reading) {
            state = State::Reading;
            read_doing++;
            return true;
        }

        return false;
    }

    /**
     * @brief Try to upgrade the mutex from shared to exclusive ownership.
     *
     * @pre Current coroutine already has shared ownership.
     * @return If upgrade success, return true and current coroutine has
     *         exclusive ownership, else return false and current coroutine
     *         has shared ownership, just like before calling `try_upgrade`.
    */
    bool try_upgrade() {
        std::lock_guard<std::mutex> lg(mtx);

        if (state == State::Reading && read_doing == 1) {
            read_doing = 0;
            state = State::Writing;
            return true;
        }

        return false;
    }

    /**
     * @brief Unlock the mutex.
     *
     * @pre Current coroutine must owns the mutex in any mode(shared or
     *      exclusive).
    */
    void unlock();

    /**
     * @brief Lock the mutex for exclusive ownership, block until success.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @return See try_lock_for but ignore coke::TOP_TIMEOUT.
    */
    Task<int> lock() { return lock_impl(detail::TimedWaitHelper{}); }

    /**
     * @brief Lock the mutex for exclusive ownership, block until success or
     *        `nsec` timeout.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @param nsec Max time to block.
     * @return Coroutine(coke::Task<int>) that needs to be awaited immediately.
     * @retval coke::TOP_SUCCESS If lock success.
     * @retval coke::TOP_TIMEOUT If `nsec` timeout.
     * @retval coke::TOP_ABORTED If process exit.
     * @retval Negative integer to indicate system error, almost never happens.
     * @see coke/global.h
    */
    Task<int> try_lock_for(const NanoSec &nsec) {
        return lock_impl(detail::TimedWaitHelper(nsec));
    }

    /**
     * @brief Lock the mutex for shared ownership, block until success.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @return See try_lock_for but ignore coke::TOP_TIMEOUT.
    */
    Task<int> lock_shared() {
        return lock_shared_impl(detail::TimedWaitHelper{});
    }

    /**
     * @brief Lock the mutex for shared ownership, block until success or
     *        `nsec` timeout.
     *
     * @pre Current coroutine doesn't owns the mutex in any mode(shared or
     *      exclusive).
     * @param nsec Max time to block.
     * @return See try_lock_for.
     *
    */
    Task<int> try_lock_shared_for(const NanoSec &nsec) {
        return lock_shared_impl(detail::TimedWaitHelper(nsec));
    }

protected:
    Task<int> lock_impl(detail::TimedWaitHelper helper);
    Task<int> lock_shared_impl(detail::TimedWaitHelper helper);

    bool can_lock_shared() const noexcept {
        return (state == State::Idle || state == State::Reading)
               && write_waiting == 0;
    }

    const void *rlock_addr() const noexcept {
        return (const char *)this + 1;
    }

    const void *wlock_addr() const noexcept {
        return (const char *)this + 2;
    }

private:
    std::mutex mtx;
    CountType read_doing;
    CountType read_waiting;
    CountType write_waiting;
    State state;
};

} // namespace coke

#endif // COKE_SHARED_MUTEX_H
