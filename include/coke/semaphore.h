#ifndef COKE_SEMAPHORE_H
#define COKE_SEMAPHORE_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class TimedSemaphore {
    template<typename Rep, typename Period>
    using duration = std::chrono::duration<Rep, Period>;

public:
    using count_type = uint32_t;

    explicit
    TimedSemaphore(count_type n) : TimedSemaphore(n, get_unique_id()) { }

    TimedSemaphore(count_type n, uint64_t uid)
        : count(n), waiting(0), uid(uid)
    { }

    TimedSemaphore(const TimedSemaphore &) = delete;
    TimedSemaphore &operator= (const TimedSemaphore &) = delete;
    ~TimedSemaphore() = default;

    /**
     * Get the unique id used by this timed semaphore.
    */
    uint64_t get_uid() const noexcept { return this->uid; }

    /**
     * Try to get a count, return true if success, else false.
     *
     * If true is returned, the caller should release the count later.
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
     * Release `cnt` counts, and wake up those who are waiting.
    */
    void release(count_type cnt = 1) {
        std::lock_guard<std::mutex> lg(mtx);
        count += cnt;

        // XXX It is possible to falsely awaken
        if (waiting > 0 && cnt > 0)
            cancel_sleep_by_id(uid, waiting > cnt ? cnt : waiting);
    }

    /**
     * Get a count, if there is no count, wait until there is.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_ABORTED, or any
     * negative number, see coke/global.h for more description.
     *
     * If coke::TOP_SUCCESS is returned, the caller should release it later.
    */
    Task<int> acquire() {
        detail::TimedWaitHelper h;
        return acquire_impl(h);
    }

    /**
     * Try to get a count for at most `time_duration` time.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_TIMEOUT,
     * coke::TOP_ABORTED, or any negative number, see coke/global.h for more
     * description.
     *
     * If coke::TOP_SUCCESS is returned, the caller should release it later.
    */
    template<typename Rep, typename Period>
    Task<int> try_acquire_for(const duration<Rep, Period> &time_duration) {
        using std::chrono::nanoseconds;
        auto nano = std::chrono::duration_cast<nanoseconds>(time_duration);
        return acquire_impl(detail::TimedWaitHelper(nano));
    }

protected:
    /**
     * The `helper` variable must not be a reference to ensure that it exists
     * until the coroutine ends.
    */
    Task<int> acquire_impl(detail::TimedWaitHelper helper);

private:
    std::mutex mtx;
    count_type count;
    count_type waiting;

    uint64_t uid;
};

} // namespace coke

#endif // COKE_SEMAPHORE_H
