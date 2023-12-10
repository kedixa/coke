#ifndef COKE_MUTEX_H
#define COKE_MUTEX_H

#include "coke/semaphore.h"

namespace coke {

class TimedMutex {
    template<typename Rep, typename Period>
    using duration = std::chrono::duration<Rep, Period>;

    template<typename Clock, typename Duration>
    using time_point = std::chrono::time_point<Clock, Duration>;

public:
    TimedMutex() : sem(1) { }

    explicit TimedMutex(uint64_t uid) : sem(1, uid) { }

    TimedMutex(const TimedMutex &) = delete;
    TimedMutex &operator= (const TimedMutex &) = delete;
    ~TimedMutex() = default;

    /**
     * Get the unique id used by this timed mutex.
    */
    uint64_t get_uid() const { return sem.get_uid(); }

    /**
     * Try to lock the mutex without block, return true if success, else false.
    */
    bool try_lock() { return sem.try_acquire(); }

    /**
     * Unlock the mutex.
    */
    void unlock() { return sem.release(1); }

    /**
     * Lock the mutex, if locked by another coroutine, this coroutine will wait
     * until the mutex is acquired.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_ABORTED, or any
     * negative number, see coke/global.h for more description.
    */
    Task<int> lock() { return sem.acquire(); }

    /**
     * Lock the mutex, if locked by another coroutine, this coroutine will wait
     * until the mutex is acquired or timeout.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_TIMEOUT,
     * coke::TOP_ABORTED, or any negative number, see coke/global.h for more
     * description.
    */
    template<typename Rep, typename Period>
    Task<int> try_lock_for(const duration<Rep, Period> &time_duration) {
        return sem.try_acquire_for(time_duration);
    }

    /**
     * Lock the mutex, if locked by another coroutine, this coroutine will wait
     * until the mutex is acquired or timeout.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_TIMEOUT,
     * coke::TOP_ABORTED, or any negative number, see coke/global.h for more
     * description.
    */
    template<typename Clock, typename Duration>
    Task<int> try_lock_until(const time_point<Clock, Duration> &time_point) {
        return sem.try_acquire_until(time_point);
    }

private:
    TimedSemaphore sem;
};

} // namespace coke

#endif // COKE_MUTEX_H
