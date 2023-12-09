#ifndef COKE_SEMAPHORE_H
#define COKE_SEMAPHORE_H

#include <mutex>
#include <cstdint>

#include "coke/sleep.h"

namespace coke {

class TimedSemaphore {
public:
    using count_type = uint32_t;

    explicit
    TimedSemaphore(count_type n) : TimedSemaphore(n, get_unique_id()) { }
    TimedSemaphore(count_type n, uint64_t uid) : count(n), waiting(0), uid(uid) { }

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
     * If coke::TOP_SUCCESS is returned, the caller should release the count later.
    */
    Task<int> acquire() {
        auto get_duration = []() { return std::chrono::seconds(10); };
        return acquire_impl(get_duration);
    }

    /**
     * Try to get a count for at most `duration` time.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_TIMEOUT, coke::TOP_ABORTED,
     * or any negative number, see coke/global.h for more description.
     *
     * If coke::TOP_SUCCESS is returned, the caller should release the count later.
    */
    template<typename Rep, typename Period>
    Task<int> acquire_for(const std::chrono::duration<Rep, Period> &duration) {
        using std::chrono::steady_clock;
        return acquire_until(steady_clock::now() + duration);
    }

    /**
     * Try to get a count until `time_point`.
     *
     * The return value may be coke::TOP_SUCCESS, coke::TOP_TIMEOUT, coke::TOP_ABORTED,
     * or any negative number, see coke/global.h for more description.
     *
     * If coke::TOP_SUCCESS is returned, the caller should release the count later.
    */
    template<typename Clock, typename Duration>
    Task<int> acquire_until(const std::chrono::time_point<Clock, Duration> &time_point) {
        auto get_duration = [time_point]() { return time_point - Clock::now(); };
        return acquire_impl(get_duration);
    }

protected:
    template<typename GetDuration>
    Task<int> acquire_impl(GetDuration get_duration);

private:
    std::mutex mtx;
    count_type count;
    count_type waiting;

    uint64_t uid;
};

template<typename GetDuration>
Task<int> TimedSemaphore::acquire_impl(GetDuration get_duration) {
    constexpr std::chrono::nanoseconds zero(0);

    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    // Directly get one if it is enough,
    if (waiting < count) {
        --count;
        co_return TOP_SUCCESS;
    }

    // otherwise reschedule to ensure relative fairness(but not absolutely).
    while (true) {
        auto dur = get_duration();
        if (dur <= zero)
            co_return TOP_TIMEOUT;

        SleepAwaiter s = sleep(uid, dur, insert_head);
        ++waiting;
        insert_head = true;

        lk.unlock();
        ret = co_await s;
        lk.lock();
        --waiting;

        if (count > 0)
            break;
        else if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    --count;
    co_return TOP_SUCCESS;
}

} // namespace coke

#endif // COKE_SEMAPHORE_H
