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

#include "coke/semaphore.h"
#include "coke/shared_mutex.h"

namespace coke {

Task<int> TimedSemaphore::acquire_impl(detail::TimedWaitHelper helper) {
    constexpr NanoSec zero(0);

    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    // Directly get one if it is enough,
    if (waiting < count) {
        --count;
        co_return TOP_SUCCESS;
    }

    // otherwise reschedule to ensure relative fairness(but not absolutely).
    while (true) {
        auto dur = helper.time_left();
        if (dur <= zero)
            co_return TOP_TIMEOUT;

        SleepAwaiter s = helper.infinite()
                         ? sleep(uid, InfiniteDuration{}, insert_head)
                         : sleep(uid, dur, insert_head);
        ++waiting;
        insert_head = true;

        lk.unlock();
        ret = co_await s;
        lk.lock();
        --waiting;

        if (count > 0)
            break;
        else if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    --count;
    co_return TOP_SUCCESS;
}


// SharedTimedMutex Implement

void SharedTimedMutex::unlock() {
    std::lock_guard<std::mutex> lg(mtx);

    if (state == State::Writing) {
        state = State::Idle;

        if (write_waiting)
            cancel_sleep_by_id(wid, 1);
        else if (read_waiting)
            cancel_sleep_by_id(rid);
    }
    else if (state == State::Reading) {
        read_doing--;

        if (read_doing == 0) {
            state = State::Idle;

            if (write_waiting)
                cancel_sleep_by_id(wid, 1);
        }
    }
}

Task<int> SharedTimedMutex::lock_impl(detail::TimedWaitHelper helper) {
    constexpr NanoSec zero(0);

    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    if (state == State::Idle && write_waiting == 0) {
        state = State::Writing;
        co_return TOP_SUCCESS;
    }

    while (true) {
        auto dur = helper.time_left();
        if (dur <= zero) {
            if (write_waiting == 0 && read_waiting > 0)
                cancel_sleep_by_id(rid);

            co_return TOP_TIMEOUT;
        }

        SleepAwaiter s = helper.infinite()
                         ? sleep(wid, InfiniteDuration{}, insert_head)
                         : sleep(wid, dur, insert_head);
        ++write_waiting;
        insert_head = true;

        lk.unlock();
        ret = co_await s;
        lk.lock();
        --write_waiting;

        if (state == State::Idle)
            break;
        else if (ret == SLEEP_ABORTED || ret < 0) {
            if (write_waiting == 0 && read_waiting > 0)
                cancel_sleep_by_id(rid);

            co_return ret;
        }
    }

    state = State::Writing;
    co_return TOP_SUCCESS;
}

Task<int> SharedTimedMutex::lock_shared_impl(detail::TimedWaitHelper helper) {
    constexpr NanoSec zero(0);

    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    if (can_lock_shared()) {
        state = State::Reading;
        read_doing++;
        co_return TOP_SUCCESS;
    }

    while (true) {
        auto dur = helper.time_left();
        if (dur <= zero)
            co_return TOP_TIMEOUT;

        SleepAwaiter s = helper.infinite()
                         ? sleep(rid, InfiniteDuration{}, insert_head)
                         : sleep(rid, dur, insert_head);
        ++read_waiting;
        insert_head = true;

        lk.unlock();
        ret = co_await s;
        lk.lock();
        --read_waiting;

        if (can_lock_shared())
            break;
        else if (ret == SLEEP_ABORTED || ret < 0)
            co_return ret;
    }

    state = State::Reading;
    read_doing++;
    co_return TOP_SUCCESS;
}

} // namespace coke
