#ifndef COKE_SERIES_H
#define COKE_SERIES_H

#include "coke/detail/awaiter_base.h"
#include "workflow/Workflow.h"

namespace coke {

class SeriesAwaiter : public BasicAwaiter<SeriesWork *> {
private:
    SeriesAwaiter();

    friend SeriesAwaiter current_series();
    friend SeriesAwaiter empty();
};

class ParallelAwaiter : public BasicAwaiter<const ParallelWork *> {
private:
    ParallelAwaiter(ParallelWork *par) {
        auto *info = this->get_info();
        par->set_callback([info](const ParallelWork *par) {
            auto *awaiter = info->get_awaiter<ParallelAwaiter>();
            awaiter->emplace_result(par);
            awaiter->done();
        });

        this->set_task(par);
    }

    friend ParallelAwaiter wait_parallel(ParallelWork *);
};


using EmptyAwaiter = SeriesAwaiter;

[[nodiscard]]
inline SeriesAwaiter current_series() { return SeriesAwaiter(); }

[[nodiscard]]
inline EmptyAwaiter empty() { return EmptyAwaiter(); }

[[nodiscard]]
inline ParallelAwaiter wait_parallel(ParallelWork *parallel) {
    return ParallelAwaiter(parallel);
}

} // namespace coke

#endif // COKE_SERIES_H
