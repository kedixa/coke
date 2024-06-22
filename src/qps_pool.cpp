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

#include <chrono>
#include <cmath>
#include <cassert>

#include "coke/qps_pool.h"

namespace coke {

static int64_t __get_current_nano() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

QpsPool::QpsPool(long query, long seconds) {
    last_nano = 0;
    last_sub = 0;

    reset_qps(query, seconds);
}

void QpsPool::reset_qps(long query, long seconds) noexcept {
    assert(query >= 0 && seconds >= 1);

    std::lock_guard<std::mutex> lg(mtx);

    if (query == 0) {
        interval_nano = 0;
        interval_sub = 0;
    }
    else {
        double d = 1e9 * seconds / query;
        double f = std::floor(d);
        d -= f;

        interval_nano = NanoType(f);
        interval_sub = NanoType(d * SUB_MASK);
    }
}

QpsPool::AwaiterType
QpsPool::get_if(unsigned count, NanoSec nsec) noexcept {
    std::lock_guard<std::mutex> lg(mtx);
    NanoType current, next_nano, next_sub;

    next_nano = last_nano + interval_nano * count;
    next_sub = last_sub + interval_sub * count;

    next_nano += next_sub / SUB_MASK;
    next_sub %= SUB_MASK;

    current = __get_current_nano();

    auto nano = NanoSec(next_nano - current);
    if (nano.count() > 0) {
        if (nano < nsec) {
            last_nano = next_nano;
            last_sub = next_sub;
            return AwaiterType(nano);
        }
        else {
            return AwaiterType(SleepAwaiter::ImmediateTag{}, SLEEP_CANCELED);
        }
    }
    else {
        last_nano = current;
        last_sub = 0;
        // SLEEP_SUCCESS
        return AwaiterType();
    }
}

} // namespace coke
