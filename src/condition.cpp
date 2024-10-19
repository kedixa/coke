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

Task<int> Condition::wait_impl(std::unique_lock<std::mutex> &lock,
                               detail::TimedWaitHelper helper) {
    int ret;
    if (helper.timeout())
        co_return TOP_TIMEOUT;

    auto s = sleep(get_addr(), helper);
    ++wait_cnt;

    lock.unlock();
    ret = co_await std::move(s);
    lock.lock();
    --wait_cnt;

    if (ret == SLEEP_SUCCESS)
        ret = TOP_TIMEOUT;
    else if (ret == SLEEP_CANCELED)
        ret = TOP_SUCCESS;

    co_return ret;
}

Task<int> Condition::wait_impl(std::unique_lock<std::mutex> &lock,
                               detail::TimedWaitHelper helper,
                               std::function<bool()> pred) {
    bool insert_head = false;
    int ret;

    while (!pred()) {
        if (helper.timeout())
            co_return TOP_TIMEOUT;

        auto s = sleep(get_addr(), helper, insert_head);
        ++wait_cnt;
        insert_head = true;

        lock.unlock();
        ret = co_await std::move(s);
        lock.lock();
        --wait_cnt;

        // if there is an error, return it
        if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    co_return TOP_SUCCESS;
}

} // namespace coke
