#ifndef COKE_SEMAPHORE_H
#define COKE_SEMAPHORE_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class TimedSemaphore {
public:
    template<typename Rep, typename Period>
    using Duration = std::chrono::duration<Rep, Period>;

    using count_type = uint32_t;

    /**
     * @brief Create a TimedSemaphore, with initial count `n`, the uid is
     *        automatically obtained from `coke::get_unique_id`.
    */
    explicit TimedSemaphore(count_type n)
        : TimedSemaphore(n, INVALID_UNIQUE_ID)
    { }

    /**
     * @brief Create a TimedSemaphore, with initial count `n`, the uid is
     *        specified by user.
    */
    TimedSemaphore(count_type n, uint64_t uid)
        : count(n), waiting(0), uid(uid)
    {
        if (this->uid == INVALID_UNIQUE_ID)
            this->uid = get_unique_id();
    }

    /**
     * @brief TimedSemaphore is not copy or move constructible.
    */
    TimedSemaphore(const TimedSemaphore &) = delete;
    TimedSemaphore &operator= (const TimedSemaphore &) = delete;

    ~TimedSemaphore() = default;

    /**
     * @brief Get the unique id.
    */
    uint64_t get_uid() const noexcept { return this->uid; }

    /**
     * @brief Try to acquire a count.
     *
     * @return Return true if acquire success, else false.
     *
     * @pre Current coroutine doesn't owns all the counts.
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
     * @brief Release `cnt` counts, and wake up those who are waiting.
    */
    void release(count_type cnt = 1) {
        std::lock_guard<std::mutex> lg(mtx);
        count += cnt;

        if (waiting > 0 && cnt > 0)
            cancel_sleep_by_id(uid, waiting > cnt ? cnt : waiting);
    }

    /**
     * @brief Acquire a count, block until success.
     *
     * @return An awaitable object that needs to be awaited immediately. The
     *         await result is an integer, which may be coke::TOP_SUCCESS,
     *         coke::TOP_TIMEOUT(for try_acquire_for), coke::TOP_ABORTED or any
     *         negative number. See `coke/global.h` for more description.
     *
     * @pre Current coroutine doesn't owns all the count.
    */
    [[nodiscard]]
    Task<int> acquire() { return acquire_impl(detail::TimedWaitHelper{}); }

    /**
     * @brief Acquire a count, block until success or `time_duration` timeout.
     *
     * @return See `acquire`.
     *
     * @param time_duration Max time to block, should be an instance of
     *        std::chrono::duration.
     *
     * @pre Current coroutine doesn't owns all the count.
    */
    template<typename Rep, typename Period>
    [[nodiscard]]
    Task<int> try_acquire_for(const Duration<Rep, Period> &time_duration) {
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
