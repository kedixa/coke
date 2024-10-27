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

#include "coke/detail/timer_task.h"
#include "coke/sleep.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

static int get_sleep_state(int state, int error) {
    switch (state) {
    case WFT_STATE_SUCCESS: return SLEEP_SUCCESS;
    case WFT_STATE_ABORTED: return SLEEP_ABORTED;
    case WFT_STATE_SYS_ERROR:
        if (error == ECANCELED)
            return SLEEP_CANCELED;
        [[fallthrough]];
    default:
        return -error;
    }
}

static NanoSec to_nsec(double sec) {
    auto dur = std::chrono::duration<double>(sec);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
}

static std::pair<time_t, long> split_nano(NanoSec nano) {
    constexpr uint64_t NANO = 1'000'000'000;
    NanoSec::rep cnt = nano.count();

    if (cnt < 0)
        return {0, 0};
    else
        return {(time_t)(cnt / NANO), (long)(cnt % NANO)};
}


namespace detail {

SleepBase::SleepBase(SleepBase &&that) noexcept
    : AwaiterBase(std::move(that)),
      timer(std::exchange(that.timer, nullptr)),
      result(std::exchange(that.result, -1))
{
    if (this->timer)
        ((TimerTask *)this->timer)->set_awaiter(this);
}

SleepBase &SleepBase::operator=(SleepBase &&that) noexcept {
    if (this != &that) {
        AwaiterBase::operator=(std::move(that));
        std::swap(this->result, that.result);
        std::swap(this->timer, that.timer);

        if (this->timer)
            ((TimerTask *)this->timer)->set_awaiter(this);

        if (that.timer)
            ((TimerTask *)that.timer)->set_awaiter(&that);
    }

    return *this;
}

int SleepBase::await_resume() {
    if (this->timer)
        return ((TimerTask *)this->timer)->get_result();
    return result;
}

int TimerTask::get_result() {
    return get_sleep_state(this->state, this->error);
}

void YieldTask::handle(int state, int error) {
    if (state == WFT_STATE_SYS_ERROR && error == ECANCELED) {
        state = WFT_STATE_SUCCESS;
        error = 0;
    }

    return this->TimerTask::handle(state, error);
}

TimerTask *create_timer(NanoSec nsec) {
    CommScheduler *s = WFGlobal::get_scheduler();
    return new TimerTask(s, nsec);
}

TimerTask *create_yield_timer() {
    CommScheduler *s = WFGlobal::get_scheduler();
    return new YieldTask(s);
}

} // namespace coke::detail


SleepAwaiter::SleepAwaiter(NanoSec nsec) {
    auto *time_task = detail::create_timer(nsec);
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}

SleepAwaiter::SleepAwaiter(double sec)
    : SleepAwaiter(to_nsec(sec))
{ }

SleepAwaiter::SleepAwaiter(uint64_t id, NanoSec nsec, bool insert_head) {
    auto *time_task = detail::create_timer(id, nsec, insert_head);
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}

SleepAwaiter::SleepAwaiter(uint64_t id, double sec, bool insert_head)
    : SleepAwaiter(id, to_nsec(sec), insert_head)
{ }

SleepAwaiter::SleepAwaiter(uint64_t id, InfiniteDuration, bool insert_head) {
    auto *time_task = detail::create_infinite_timer(id, insert_head);
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}

SleepAwaiter::SleepAwaiter(const void *addr, NanoSec nsec, bool insert_head) {
    auto *time_task = detail::create_timer(addr, nsec, insert_head);
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}

SleepAwaiter::SleepAwaiter(const void *addr, double sec, bool insert_head)
    : SleepAwaiter(addr, to_nsec(sec), insert_head)
{ }

SleepAwaiter::SleepAwaiter(const void *addr, InfiniteDuration,
                           bool insert_head) {
    auto *time_task = detail::create_infinite_timer(addr, insert_head);
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}

SleepAwaiter::SleepAwaiter(ImmediateTag, int state) {
    this->result = state;
}

SleepAwaiter::SleepAwaiter(YieldTag) {
    auto *time_task = detail::create_yield_timer();
    time_task->set_awaiter(this);
    this->timer = time_task;
    this->set_task(time_task);
}


WFSleepAwaiter::WFSleepAwaiter(const std::string &name, NanoSec nano) {
    auto cb = [info = this->get_info()](WFTimerTask *task) {
        auto *awaiter = info->get_awaiter<WFSleepAwaiter>();
        int state = task->get_state();
        int error = task->get_error();
        awaiter->emplace_result(get_sleep_state(state, error));
        awaiter->done();
    };

    auto [sec, nsec] = split_nano(nano);
    set_task(WFTaskFactory::create_timer_task(name, sec, nsec, cb));
}

int cancel_sleep_by_name(const std::string &name, std::size_t max) {
    return WFTaskFactory::cancel_by_name(name, max);
}

} // namespace coke
