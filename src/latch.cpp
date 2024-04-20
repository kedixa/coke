#include "coke/latch.h"

#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

LatchAwaiter::LatchAwaiter(SubTask *task) {
    WFCounterTask *counter = static_cast<WFCounterTask *>(task);
    AwaiterInfo<void> *info = this->get_info();

    counter->set_callback([info](WFCounterTask *) {
        auto *awaiter = info->get_awaiter<LatchAwaiter>();
        awaiter->done();
    });

    set_task(counter);
}

void Latch::count_down(long n) noexcept {
    std::vector<SubTask *> wake;

    {
        std::lock_guard<std::mutex> lg(this->mtx);
        this->expected -= n;

        if (this->expected <= 0)
            wake = std::move(tasks);
    }

    if (!wake.empty())
        wake_up(std::move(wake));

    // ATTENTION: *this maybe destroyed
}

LatchAwaiter Latch::create_awaiter(long n) noexcept {
    std::vector<SubTask *> wake;
    SubTask *task = nullptr;

    {
        std::lock_guard<std::mutex> lg(this->mtx);
        this->expected -= n;

        if (this->expected <= 0)
            wake = std::move(tasks);
        else {
            task = WFTaskFactory::create_counter_task(1, nullptr);
            tasks.push_back(task);
        }
    }

    if (!wake.empty())
        wake_up(std::move(wake));

    // ATTENTION: *this maybe destroyed

    return task ? LatchAwaiter(task) : LatchAwaiter();
}

void Latch::wake_up(std::vector<SubTask *> tasks) noexcept {
    // TODO: Switching to other threads to wake up the waiter can avoid
    // the current coroutine waiting for the wake-up to complete, but
    // which thread to switch to is a problem

    for (SubTask *task : tasks) {
        WFCounterTask *counter = static_cast<WFCounterTask *>(task);
        counter->count();
    }
}

void SyncLatch::wait() const noexcept {
    if (!lt.try_wait()) {
        int cookie = WFGlobal::sync_operation_begin();
        lt.wait();
        WFGlobal::sync_operation_end(cookie);
    }
}

} // namespace coke
