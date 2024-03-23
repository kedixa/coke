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
     * @return An awaitable object that needs to be awaited immediately. The
     *         await result is an integer, which may be coke::TOP_SUCCESS,
     *         coke::TOP_TIMEOUT(for try_lock_for), coke::TOP_ABORTED or any
     *         negative number. See `coke/global.h` for more description.
     *
     * @pre Current coroutine doesn't owns the mutex.
    */
    [[nodiscard]]
    Task<int> lock() { return sem.acquire(); }

    /**
     * @brief Lock the mutex, block until success or `nsec` timeout.
     *
     * @return See `lock`.
     *
     * @param nsec Max time to block.
     *
     * @pre Current coroutine doesn't owns the mutex.
    */
    [[nodiscard]]
    Task<int> try_lock_for(const NanoSec &nsec) {
        return sem.try_acquire_for(nsec);
    }

private:
    TimedSemaphore sem;
};

} // namespace coke

#endif // COKE_MUTEX_H
