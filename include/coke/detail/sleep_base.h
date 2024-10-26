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

#ifndef COKE_DETAIL_SLEEP_BASE_H
#define COKE_DETAIL_SLEEP_BASE_H

#include <chrono>

#include "coke/detail/awaiter_base.h"

namespace coke::detail {

using NanoSec = std::chrono::nanoseconds;

class SleepBase : public AwaiterBase {
public:
    SleepBase(SleepBase &&that) noexcept;
    SleepBase &operator= (SleepBase &&that) noexcept;
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

    TimedWaitHelper(NanoSec nano)
        : abs_time(now() + nano)
    { }

    bool infinite() const { return abs_time == max(); }

    NanoSec time_left() const { return abs_time - now(); }

    bool timeout() const { return abs_time <= now(); }

private:
    TimePoint abs_time;
};

} // namespace coke::detail

#endif // COKE_DETAIL_SLEEP_BASE_H
