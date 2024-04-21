#ifndef COKE_DETAIL_SLEEP_BASE_H
#define COKE_DETAIL_SLEEP_BASE_H

#include <chrono>

#include "coke/detail/awaiter_base.h"

namespace coke::detail {

using NanoSec = std::chrono::nanoseconds;

class SleepBase : public AwaiterBase {
public:
    SleepBase(SleepBase &&that);
    SleepBase &operator= (SleepBase &&that);
    ~SleepBase() = default;

    int await_resume();

protected:
    SleepBase() : timer(nullptr), result(-1) { }

protected:
    SubTask *timer;
    int result;
};


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

} // namespace coke::detail

#endif // COKE_DETAIL_SLEEP_BASE_H
