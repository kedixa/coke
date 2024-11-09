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

#include <system_error>

#include "coke/semaphore.h"

namespace coke {

class Mutex {
public:
    /**
     * @brief Create a Mutex.
    */
    Mutex() noexcept : sem(1) { }

    /**
     * @brief Mutex is neither copyable nor movable.
    */
    Mutex(const Mutex &) = delete;
    Mutex &operator= (const Mutex &) = delete;

    ~Mutex() = default;

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
    Task<int> try_lock_for(NanoSec nsec) {
        return sem.try_acquire_for(nsec);
    }

private:
    Semaphore sem;
};

template<typename M>
class UniqueLock {
public:
    using MutexType = M;

    UniqueLock() noexcept : co_mtx(nullptr), owns(false) { }

    /**
     * @brief Create UniqueLock with mutex m.
     *
     * @param m The mutex to wrap.
     * @param is_locked Whether m is already locked.
     */
    explicit UniqueLock(MutexType &m, bool is_locked = false) noexcept
        : co_mtx(std::addressof(m)), owns(is_locked)
    { }

    /**
     * @brief Move construct from other.
     */
    UniqueLock(UniqueLock &&other) noexcept
        : co_mtx(std::exchange(other.co_mtx, nullptr)),
          owns(std::exchange(other.owns, false))
    { }

    /**
     * @brief Unlock *this and move assign from other.
     */
    UniqueLock &operator= (UniqueLock &&other) {
        if (owns)
            co_mtx->unlock();

        co_mtx = std::exchange(other.co_mtx, nullptr);
        owns = std::exchange(other.owns, false);
    }

    /**
     * @brief Unlock the mutex if owns the lock.
     */
    ~UniqueLock() {
        if (owns)
            co_mtx->unlock();
    }

    /**
     * @brief Test whether the lock owns its associated mutex.
     */
    bool owns_lock() const noexcept {
        return owns;
    }

    void swap(UniqueLock &other) noexcept {
        if (this != &other) {
            std::swap(this->co_mtx, other.co_mtx);
            std::swap(this->owns, other.owns);
        }
    }

    /**
     * @brief Disassociates the associated mutex without unlocking it.
     */
    MutexType *release() noexcept {
        M *m = co_mtx;
        co_mtx = nullptr;
        owns = false;
        return m;
    }

    /**
     * @brief Try to lock the mutex without blocking.
     * @throw std::system_error if already locked.
     */
    bool try_lock() {
        if (owns)
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

        owns = co_mtx->try_lock();
        return owns;
    }

    /**
     * @brief Lock the mutex, see Mutex::lock.
     * @throw std::system_error if already locked.
     */
    Task<int> lock() {
        if (owns)
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

        int ret = co_await co_mtx->lock();
        if (ret == coke::TOP_SUCCESS)
            owns = true;

        co_return ret;
    }

    /**
     * @brief Try to lock the mutex, wait at most nsec, see Mutex::try_lock_for.
     * @throw std::system_error if already locked.
     */
    Task<int> try_lock_for(NanoSec nsec) {
        if (owns)
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

        int ret = co_await co_mtx->try_lock_for(nsec);
        if (ret == coke::TOP_SUCCESS)
            owns = true;

        co_return ret;
    }

    /**
     * @brief Unlock the associated mutex.
     * @throw std::system_error if not owns lock.
     */
    void unlock() {
        if (!owns)
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));

        co_mtx->unlock();
        owns = false;
    }

private:
    MutexType *co_mtx;
    bool owns;
};

} // namespace coke

#endif // COKE_MUTEX_H
