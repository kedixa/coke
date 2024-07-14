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
        if (helper.timeout())
            co_return TOP_TIMEOUT;

        auto s = sleep(get_addr(), helper, insert_head);
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
            cancel_sleep_by_addr(wlock_addr(), 1);
        else if (read_waiting)
            cancel_sleep_by_addr(rlock_addr());
    }
    else if (state == State::Reading) {
        read_doing--;

        if (read_doing == 0) {
            state = State::Idle;

            if (write_waiting)
                cancel_sleep_by_addr(wlock_addr(), 1);
        }
    }
}

Task<int> SharedTimedMutex::lock_impl(detail::TimedWaitHelper helper) {
    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    if (state == State::Idle && write_waiting == 0) {
        state = State::Writing;
        co_return TOP_SUCCESS;
    }

    while (true) {
        if (helper.timeout()) {
            if (write_waiting == 0 && read_waiting > 0)
                cancel_sleep_by_addr(rlock_addr());

            co_return TOP_TIMEOUT;
        }

        auto s = sleep(wlock_addr(), helper, insert_head);
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
                cancel_sleep_by_addr(rlock_addr());

            co_return ret;
        }
    }

    state = State::Writing;
    co_return TOP_SUCCESS;
}

Task<int> SharedTimedMutex::lock_shared_impl(detail::TimedWaitHelper helper) {
    std::unique_lock<std::mutex> lk(mtx);
    bool insert_head = false;
    int ret;

    if (can_lock_shared()) {
        state = State::Reading;
        read_doing++;
        co_return TOP_SUCCESS;
    }

    while (true) {
        if (helper.timeout())
            co_return TOP_TIMEOUT;

        auto s = sleep(rlock_addr(), helper, insert_head);
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
