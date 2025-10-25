/**
 * Copyright 2024-2025 Coke Project (https://github.com/kedixa/coke)
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

#include "coke/detail/condition_impl.h"

namespace coke::detail {

Task<int> cv_wait_impl(std::unique_lock<std::mutex> &lock, const void *addr,
                       TimedWaitHelper helper, int *wait_cnt)
{
    if (helper.timeout())
        co_return TOP_TIMEOUT;

    int ret;
    auto s = sleep(addr, helper);

    if (wait_cnt)
        ++*wait_cnt;

    lock.unlock();
    ret = co_await std::move(s);
    lock.lock();

    if (wait_cnt)
        --*wait_cnt;

    if (ret == SLEEP_SUCCESS)
        ret = TOP_TIMEOUT;
    else if (ret == SLEEP_CANCELED)
        ret = TOP_SUCCESS;

    co_return ret;
}

Task<int> cv_wait_impl(std::unique_lock<std::mutex> &lock, const void *addr,
                       TimedWaitHelper helper, std::function<bool()> pred,
                       int *wait_cnt)
{
    int ret;
    bool insert_head = false;

    while (!pred()) {
        if (helper.timeout())
            co_return TOP_TIMEOUT;

        auto s = sleep(addr, helper, insert_head);
        insert_head = true;

        if (wait_cnt)
            ++*wait_cnt;

        lock.unlock();
        ret = co_await std::move(s);
        lock.lock();

        if (wait_cnt)
            --*wait_cnt;

        // if there is an error, return it
        if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    co_return TOP_SUCCESS;
}

} // namespace coke::detail
