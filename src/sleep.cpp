#include "coke/sleep.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

SleepAwaiter::SleepAwaiter() {
    emplace_result(0);
}

SleepAwaiter::SleepAwaiter(long sec, long nsec) {
    auto cb = [info = this->get_info()](WFTimerTask *task) {
        auto *awaiter = info->get_awaiter<SleepAwaiter>();
        awaiter->emplace_result(task->get_state());
        awaiter->done();
    };

    set_task(WFTaskFactory::create_timer_task(sec, nsec, cb));
}

} // namespace
