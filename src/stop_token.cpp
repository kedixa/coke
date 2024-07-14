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

#include "coke/stop_token.h"

namespace coke {

Task<bool> StopToken::wait_finish_impl(detail::TimedWaitHelper helper) {
    std::unique_lock<std::mutex> lk(mtx);
    while (n != 0 && !helper.timeout()) {
        SleepAwaiter s = sleep(get_finish_addr(), helper);

        lk.unlock();
        int ret = co_await s;
        lk.lock();

        if (ret < 0 || ret == SLEEP_ABORTED)
            break;
    }

    co_return n == 0;
}

Task<bool> StopToken::wait_stop_impl(detail::TimedWaitHelper helper) {
    std::unique_lock<std::mutex> lk(mtx);
    while (!stop_requested() && !helper.timeout()) {
        SleepAwaiter s = sleep(get_stop_addr(), helper);

        lk.unlock();
        int ret = co_await s;
        lk.lock();

        if (ret < 0 || ret == SLEEP_ABORTED)
            break;
    }

    co_return stop_requested();
}

} // namespace coke
