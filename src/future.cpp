#include "coke/future.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

namespace detail {

void FutureStateBase::wakeup(int state) {
    WFCounterTask *task;

    {
        std::lock_guard<std::mutex> lg(this->mtx);
        task = static_cast<WFCounterTask *>(this->counter);
        this->counter = nullptr;

        if (state == FUTURE_STATE_READY)
            this->ready = true;
    }

    if (task) {
        task->user_data = reinterpret_cast<void *>(intptr_t(state));
        task->count();
    }
}

SubTask *FutureStateBase::create_wait_task(int &state) {
    WFCounterTask *task;
    std::lock_guard<std::mutex> lg(this->mtx);

    // At most one waiting, it is illegal to call this function when counter is not nullptr
    if (this->ready || this->counter) {
        state = this->ready ? FUTURE_STATE_READY : FUTURE_STATE_INVALID;
        return nullptr;
    }

    task = WFTaskFactory::create_counter_task(1, nullptr);
    task->user_data = reinterpret_cast<void *>(intptr_t(FUTURE_STATE_INVALID));
    this->counter = task;

    return task;
}

} // namespace detail

FutureAwaiter::FutureAwaiter(SubTask *task) {
    WFCounterTask *counter = static_cast<WFCounterTask *>(task);

    auto cb = [info = this->get_info()](WFCounterTask *task) {
        auto *awaiter = info->get_awaiter<FutureAwaiter>();
        intptr_t ret = reinterpret_cast<intptr_t>(task->user_data);
        int res = static_cast<int>(ret);

        awaiter->emplace_result(res);
        awaiter->done();
    };

    counter->set_callback(std::move(cb));

    set_task(task);
}

FutureAwaiter::FutureAwaiter(int ret) {
    emplace_result(ret);
}

} // namespace coke
