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
    enum class State : std::uint8_t {
        Idle        = 0,
        Reading     = 1,
        Writing     = 2,
    };

public:
    using count_type = std::uint32_t;

    /**
     * @brief Create a SharedTimedMutex, the rid/wid is automatically
     *        obtained from `coke::get_unique_id`.
    */
    SharedTimedMutex()
        : SharedTimedMutex(INVALID_UNIQUE_ID, INVALID_UNIQUE_ID)
    { }

    /**
     * @brief Create a SharedTimedMutex with rid/wid specified by user.
    */
    SharedTimedMutex(uint64_t rid, uint64_t wid)
        : read_doing(0), read_waiting(0), write_waiting(0),
          state(State::Idle), rid(rid), wid(wid)
    {
        if (this->rid == INVALID_UNIQUE_ID)
            this->rid = get_unique_id();
        if (this->wid == INVALID_UNIQUE_ID)
            this->wid = get_unique_id();
    }

    /**
     * @brief SharedTimedMutex is not copy or move constructible.
    */
    SharedTimedMutex(const SharedTimedMutex &) = delete;
    SharedTimedMutex &operator= (const SharedTimedMutex &) = delete;

    ~SharedTimedMutex() = default;

    /**
     * @brief Get the read unique id.
    */
    uint64_t get_rid() const { return rid; }

    /**
     * @brief Get the write unique id.
    */
    uint64_t get_wid() const { return wid; }

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

private:
    std::mutex mtx;
    count_type read_doing;
    count_type read_waiting;
    count_type write_waiting;
    State state;

    uint64_t rid;
    uint64_t wid;
};

} // namespace coke

#endif // COKE_SHARED_MUTEX_H
