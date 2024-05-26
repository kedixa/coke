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

#include "coke/condition.h"

namespace coke {

Task<int> TimedCondition::wait_impl(std::unique_lock<std::mutex> &lock,
                                    detail::TimedWaitHelper helper) {
    constexpr NanoSec zero(0);

    int ret;
    auto dur = helper.time_left();
    if (dur <= zero)
        co_return TOP_TIMEOUT;

    SleepAwaiter s = helper.infinite()
                     ? sleep(uid, InfiniteDuration{})
                     : sleep(uid, dur);

    lock.unlock();
    ret = co_await s;
    lock.lock();

    if (ret == SLEEP_SUCCESS)
        ret = TOP_TIMEOUT;
    else if (ret == SLEEP_CANCELED)
        ret = TOP_SUCCESS;

    co_return ret;
}

Task<int> TimedCondition::wait_impl(std::unique_lock<std::mutex> &lock,
                                    detail::TimedWaitHelper helper,
                                    std::function<bool()> pred) {
    constexpr NanoSec zero(0);

    bool insert_head = false;
    int ret;

    while (!pred()) {
        auto dur = helper.time_left();
        if (dur <= zero)
            co_return TOP_TIMEOUT;

        SleepAwaiter s = helper.infinite()
                         ? sleep(uid, InfiniteDuration{}, insert_head)
                         : sleep(uid, dur, insert_head);

        lock.unlock();
        insert_head = true;
        ret = co_await s;
        lock.lock();

        // if there is an error, return it
        if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    co_return TOP_SUCCESS;
}

} // namespace coke
