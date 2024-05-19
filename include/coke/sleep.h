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

#ifndef COKE_SLEEP_H
#define COKE_SLEEP_H

#include <string>

#include "coke/detail/sleep_base.h"
#include "coke/global.h"

namespace coke {

using NanoSec = detail::NanoSec;

/**
 * @brief InfiniteDuration is used to represent an infinite time when sleeping
 *        with id, and will only be awakened when `cancel_sleep_by_id`.
*/
struct InfiniteDuration { };


// Negative numbers indicate errors
constexpr int SLEEP_SUCCESS = 0;
constexpr int SLEEP_CANCELED = 1;
constexpr int SLEEP_ABORTED = 2;

static_assert(SLEEP_SUCCESS == TOP_SUCCESS);
static_assert(SLEEP_ABORTED == TOP_ABORTED);


class SleepAwaiter : public detail::SleepBase {
public:
    struct ImmediateTag { };
    struct YieldTag { };

    /**
     * @brief Create a SleepAwaiter that returns SLEEP_SUCCESS immediately,
     *        this awaiter will not switch thread.
    */
    SleepAwaiter() : SleepAwaiter(ImmediateTag{}, SLEEP_SUCCESS) { }

    /**
     * @brief Create a SleepAwaiter that will sleep `nsec`.
     *
     * @param nsec An instance of std::chrono::nanoseconds, representing the
     *        time to sleep. You can also use larger time units such as
     *        std::chrono::seconds.
    */
    SleepAwaiter(const NanoSec &nsec);

    /**
     * @brief Create a SleepAwaiter that will sleep `sec`.
     *
     * @param sec A double number(should be >= 0.0) representing the time to
     *        sleep. Make sure it is within a reasonable range, such as 0.5,
     *        3.14. Using bad parameters will cause incorrect behavior, such as
     *        -1, 1e100, inf, etc.
    */
    SleepAwaiter(double sec);

    /**
     * @brief Create a SleepAwaiter that will sleep `nsec` with id, this awaiter
     *        can be canceled using the same id, even it is not co awaited. All
     *        the SleepAwaiter with same id share a same queue, set
     *        `insert_head` to true will add it to the front of the queue, and
     *        will be canceled earlier.
     *
     * @param id A unique id acquired from coke::get_unique_id();.
     * @param nsec See previous description.
     * @param insert_head Whether add the SleepAwaiter to the front of the
     *        queue.
     */
    SleepAwaiter(uint64_t id, const NanoSec &nsec, bool insert_head);

    /**
     * @brief Same as sleep `nsec` with id, but use double `sec`.
    */
    SleepAwaiter(uint64_t id, double sec, bool insert_head);

    /**
     * @brief Same as previous SleepAwaiter, but sleep for infinite duration,
     *        must use cancel to wake it up.
    */
    SleepAwaiter(uint64_t id, const InfiniteDuration &, bool insert_head);

    // Inner use only
    SleepAwaiter(ImmediateTag, int state);
    SleepAwaiter(YieldTag);
};


class WFSleepAwaiter : public BasicAwaiter<int> {
public:
    /**
     * @brief WFSleepAwaiter is used to sleep by name, which is implemented in
     *        Workflow.
    */
    WFSleepAwaiter(const std::string &name, const NanoSec &nsec);
};


[[nodiscard]] inline
SleepAwaiter sleep(const NanoSec &nsec) { return SleepAwaiter(nsec); }

[[nodiscard]] inline
SleepAwaiter sleep(double sec) { return SleepAwaiter(sec); }

[[nodiscard]] inline
SleepAwaiter sleep(uint64_t id, const NanoSec &nsec, bool insert_head = false) {
    return SleepAwaiter(id, nsec, insert_head);
}

[[nodiscard]] inline
SleepAwaiter sleep(uint64_t id, double sec, bool insert_head = false) {
    return SleepAwaiter(id, sec, insert_head);
}

[[nodiscard]]
inline SleepAwaiter sleep(uint64_t id,
                          const InfiniteDuration &inf_duration,
                          bool insert_head = false)
{
    return SleepAwaiter(id, inf_duration, insert_head);
}

[[nodiscard, deprecated]] inline
SleepAwaiter sleep(long sec, long nsec) {
    auto dur = std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec);
    return SleepAwaiter(dur);
}

[[nodiscard]]
inline SleepAwaiter yield() { return SleepAwaiter(SleepAwaiter::YieldTag{}); }

std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max);

inline std::size_t cancel_sleep_by_id(uint64_t id) {
    return cancel_sleep_by_id(id, std::size_t(-1));
}

[[nodiscard]] inline
WFSleepAwaiter sleep(const std::string &name, const NanoSec &nsec) {
    return WFSleepAwaiter(name, nsec);
}

void cancel_sleep_by_name(const std::string &name, std::size_t max);

inline void cancel_sleep_by_name(const std::string &name) {
    return cancel_sleep_by_name(name, std::size_t(-1));
}

} // namespace coke

#endif // COKE_SLEEP_H
