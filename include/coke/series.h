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

using SeriesCreater = SeriesWork * (*)(SubTask *);
/**
 * @brief Set a new series creater and return the old one.
 *
 * This function must be called exactly before any coroutine is created.
 *
 * @param creater An instance of SeriesCreater which don't throw any exceptions.
 *        If creater is nullptr, default series creater is set.
 * @return Previous series creater, if this is the first call, default series
 *         creater is returned.
*/
SeriesCreater set_series_creater(SeriesCreater creater) noexcept;

/**
 * @brief Get current series creater.
 *
 * @return Same as the param of last call to `set_series_creater`, if it is
 *         never called, return default series creater.
*/
SeriesCreater get_series_creater() noexcept;

} // namespace coke

#endif // COKE_SERIES_H
