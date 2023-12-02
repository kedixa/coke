#include "coke/sleep.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

static int get_sleep_state(WFTimerTask *task) {
    switch (task->get_state()) {
    case WFT_STATE_SUCCESS: return SLEEP_SUCCESS;
    case WFT_STATE_ABORTED: return SLEEP_ABORTED;
    case WFT_STATE_SYS_ERROR:
        if (task->get_error() == ECANCELED)
            return SLEEP_CANCELED;
        [[fallthrough]];
    default:
        return -task->get_error();
    }
}

using std::chrono::nanoseconds;
WFTimerTask *create_cancelable_timer(uint64_t, bool, time_t, long, timer_callback_t);

static std::pair<time_t, long> split_nano(nanoseconds nano) {
    constexpr uint64_t NANO = 1'000'000'000;
    nanoseconds::rep cnt = nano.count();

    if (cnt < 0)
        return {0, 0};
    else
        return {cnt / NANO, cnt % NANO};
}

SleepAwaiter::SleepAwaiter(nanoseconds nano) {
    auto cb = [info = this->get_info()](WFTimerTask *task) {
        auto *awaiter = info->get_awaiter<SleepAwaiter>();
        awaiter->emplace_result(get_sleep_state(task));
        awaiter->done();
    };

    auto [sec, nsec] = split_nano(nano);
    set_task(WFTaskFactory::create_timer_task(sec, nsec, cb));
}

SleepAwaiter::SleepAwaiter(const std::string &name, nanoseconds nano) {
    auto cb = [info = this->get_info()](WFTimerTask *task) {
        auto *awaiter = info->get_awaiter<SleepAwaiter>();
        awaiter->emplace_result(get_sleep_state(task));
        awaiter->done();
    };

    auto [sec, nsec] = split_nano(nano);
    set_task(WFTaskFactory::create_timer_task(name, sec, nsec, cb));
}

void cancel_sleep_by_name(const std::string &name, std::size_t max) {
    return WFTaskFactory::cancel_by_name(name, max);
}

SleepAwaiter::SleepAwaiter(uint64_t id, nanoseconds nano, bool insert_head) {
    auto cb = [info = this->get_info()](WFTimerTask *task) {
        auto *awaiter = info->get_awaiter<SleepAwaiter>();
        awaiter->emplace_result(get_sleep_state(task));
        awaiter->done();
    };

    auto [sec, nsec] = split_nano(nano);
    set_task(create_cancelable_timer(id, insert_head, sec, nsec, cb));
}

} // namespace coke
