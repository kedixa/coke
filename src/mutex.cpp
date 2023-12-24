#include "coke/semaphore.h"

namespace coke {

Task<int> TimedSemaphore::acquire_impl(detail::TimedWaitHelper helper) {
    constexpr detail::TimedWaitHelper::duration zero(0);

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

} // namespace coke
