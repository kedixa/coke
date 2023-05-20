#include "coke/sleep.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

SleepAwaiter::SleepAwaiter(long sec, long nsec) {
    auto cb = [this](WFTimerTask *t) {
        this->state = t->get_state();
        this->done();
    };

    set_task(WFTaskFactory::create_timer_task(sec, nsec, cb));
}

} // namespace
