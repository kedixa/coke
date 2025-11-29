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

#include "coke/latch.h"

#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

void Latch::count_down(long n)
{
    const void *addr = nullptr;

    {
        std::lock_guard<std::mutex> lg(this->mtx);

        if (this->expected > 0) {
            this->expected -= n;

            if (this->expected <= 0)
                addr = get_addr();
        }
    }

    if (addr)
        cancel_sleep_by_addr(addr);

    // ATTENTION: *this maybe destroyed
}

LatchAwaiter Latch::create_awaiter(long n)
{
    LatchAwaiter a;
    const void *addr = nullptr;

    {
        std::lock_guard<std::mutex> lg(this->mtx);

        if (this->expected > 0) {
            this->expected -= n;

            if (this->expected <= 0) {
                addr = get_addr();
            }
            else {
                a = SleepAwaiter(get_addr(), coke::inf_dur, false);
            }
        }
    }

    if (addr)
        cancel_sleep_by_addr(addr);

    // ATTENTION: *this maybe destroyed
    return a;
}

LatchAwaiter Latch::wait_impl(detail::TimedWaitHelper helper)
{
    std::lock_guard<std::mutex> lg(mtx);
    if (this->expected > 0)
        return sleep(get_addr(), helper);
    else
        return SleepAwaiter();
}

} // namespace coke
