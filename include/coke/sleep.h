#ifndef COKE_SLEEP_H
#define COKE_SLEEP_H

#include <chrono>
#include <string>

#include "coke/global.h"

namespace coke {

// Negative numbers indicate errors
constexpr int SLEEP_SUCCESS = 0;
constexpr int SLEEP_CANCELED = 1;
constexpr int SLEEP_ABORTED = 2;

static_assert(SLEEP_SUCCESS == TOP_SUCCESS);
static_assert(SLEEP_ABORTED == TOP_ABORTED);


/**
 * InfiniteDuration is used to represent an infinite time when sleeping with id,
 * and will only be awakened when `cancel_sleep_by_id`.
*/
struct InfiniteDuration { };

using NanoSec = std::chrono::nanoseconds;

class SleepAwaiter : public BasicAwaiter<int> {
    static NanoSec __plus(long sec, long nsec) {
        using namespace std::chrono;
        return seconds(sec) + nanoseconds(nsec);
    }

public:
    struct InnerTag { };

    SleepAwaiter() : SleepAwaiter(InnerTag{}, SLEEP_SUCCESS) { }
    SleepAwaiter(long sec, long nsec) : SleepAwaiter(__plus(sec, nsec)) { }

    SleepAwaiter(const NanoSec &nsec);
    SleepAwaiter(const std::string &name, const NanoSec &nsec);
    SleepAwaiter(uint64_t id, const NanoSec &nsec, bool insert_head=false);
    SleepAwaiter(uint64_t id, InfiniteDuration, bool insert_head=false);

    // Inner use only
    SleepAwaiter(InnerTag, int s) { emplace_result(s); }
};


[[nodiscard]]
inline SleepAwaiter sleep(long sec, long nsec) {
    return SleepAwaiter(sec, nsec);
}

[[nodiscard]]
inline SleepAwaiter sleep(const NanoSec &nsec) {
    return SleepAwaiter(nsec);
}

[[nodiscard]]
inline
SleepAwaiter sleep(const std::string &name, const NanoSec &nsec) {
    return SleepAwaiter(name, nsec);
}

[[nodiscard]]
inline
SleepAwaiter sleep(uint64_t id, const NanoSec &nsec, bool insert_head = false) {
    return SleepAwaiter(id, nsec, insert_head);
}

[[nodiscard]]
inline SleepAwaiter sleep(uint64_t id, const InfiniteDuration &inf_duration,
                          bool insert_head = false)
{
    return SleepAwaiter(id, inf_duration, insert_head);
}

[[nodiscard]]
inline SleepAwaiter yield() {
    return SleepAwaiter(0, 0);
}

void cancel_sleep_by_name(const std::string &name, std::size_t max);

inline void cancel_sleep_by_name(const std::string &name) {
    return cancel_sleep_by_name(name, std::size_t(-1));
}

std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max);

inline std::size_t cancel_sleep_by_id(uint64_t id) {
    return cancel_sleep_by_id(id, std::size_t(-1));
}


namespace detail {

class TimedWaitHelper {
public:
    using ClockType = std::chrono::steady_clock;
    using TimePoint = ClockType::time_point;

    constexpr static TimePoint max() { return TimePoint::max(); }
    static TimePoint now() { return ClockType::now(); }

    TimedWaitHelper() : abs_time(max()) { }

    TimedWaitHelper(const NanoSec &nano)
        : abs_time(now() + nano)
    { }

    bool infinite() const { return abs_time == max(); }

    NanoSec time_left() const { return abs_time - now(); }

private:
    TimePoint abs_time;
};

} // namespace detail

} // namespace coke

#endif // COKE_SLEEP_H
