#ifndef COKE_DETAIL_TIMER_TASK_H
#define COKE_DETAIL_TIMER_TASK_H

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

TimerTask *create_timer(const NanoSec &nsec);

TimerTask *create_timer(uint64_t id, const NanoSec &nsec, bool insert_head);

TimerTask *create_infinite_timer(uint64_t id, bool insert_head);

} // namespace coke::detail

#endif // COKE_DETAIL_TIMER_TASK_H
