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

#ifndef COKE_DETAIL_TIMER_TASK_H
#define COKE_DETAIL_TIMER_TASK_H

#include <atomic>

#include "coke/detail/sleep_base.h"

#include "workflow/Workflow.h"
#include "workflow/SleepRequest.h"

namespace coke::detail {

class TimerTask : public SleepRequest {
public:
    using NanoSec = std::chrono::nanoseconds;

    TimerTask(CommScheduler *scheduler, const NanoSec &nsec)
        : SleepRequest(scheduler), awaiter(nullptr), nsec(nsec)
    { }

    virtual ~TimerTask() = default;

    int get_state() const { return this->state; }
    int get_error() const { return this->error; }

    AwaiterBase *get_awaiter() const { return awaiter; }
    void set_awaiter(AwaiterBase *awaiter) { this->awaiter = awaiter; }

    int get_result();

protected:
    virtual SubTask *done() override {
        SeriesWork *series = series_of(this);

        awaiter->done();

        delete this;
        return series->pop();
    }

    virtual int duration(struct timespec *value) override {
        constexpr int64_t N = 1'000'000'000;
        NanoSec::rep cnt = nsec.count();

        if (cnt >= 0) {
            value->tv_sec = (time_t)(cnt / N);
            value->tv_nsec = (long)(cnt % N);
        }
        else {
            value->tv_sec = 0;
            value->tv_nsec = 0;
        }

        return 0;
    }

protected:
    AwaiterBase *awaiter;
    NanoSec nsec;
};

class YieldTask : public TimerTask {
public:
    YieldTask(CommScheduler *scheduler)
        : TimerTask(scheduler, std::chrono::seconds(1)),
          cancel_done(false), ref(2)
    { }

    virtual void dispatch() override {
        if (this->scheduler->sleep(this) >= 0) {
            this->cancel();

            cancel_done.store(true, std::memory_order_release);
            cancel_done.notify_one();
        }
        else {
            cancel_done.store(true, std::memory_order_relaxed);
            this->handle(SS_STATE_ERROR, errno);
        }

        this->dec_ref();
    }

protected:
    virtual void handle(int state, int error) override;

    virtual SubTask *done() override {
        SeriesWork *series = series_of(this);

        awaiter->done();

        // sync with dispatch, this->cancel must finish before this,
        // because poller_del_timer need to read data from poller_node
        if (cancel_done.load(std::memory_order_acquire) == false)
            cancel_done.wait(false, std::memory_order_acquire);

        this->dec_ref();

        return series->pop();
    }

    void dec_ref() {
        if (ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }

private:
    std::atomic<bool> cancel_done;
    std::atomic<int> ref;
};

TimerTask *create_timer(const NanoSec &nsec);

TimerTask *create_timer(uint64_t id, const NanoSec &nsec, bool insert_head);

TimerTask *create_infinite_timer(uint64_t id, bool insert_head);

TimerTask *create_yield_timer();

} // namespace coke::detail

#endif // COKE_DETAIL_TIMER_TASK_H
