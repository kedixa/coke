#include "coke/latch.h"

#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

LatchAwaiter::LatchAwaiter(SubTask *task) {
    WFCounterTask *counter = static_cast<WFCounterTask *>(task);
    auto *info = this->get_info();

    counter->set_callback([info](WFCounterTask *) {
        auto *awaiter = info->get_awaiter<LatchAwaiter>();
        awaiter->done();
    });

    set_task(task);
}

Latch::Latch(size_t n) {
    task = WFTaskFactory::create_counter_task(n, nullptr);
}

Latch::~Latch() {
    WFCounterTask *counter = static_cast<WFCounterTask *>(task);

    if (!awaited)
        counter->dismiss();
}

void Latch::count_down() {
    WFCounterTask *counter = static_cast<WFCounterTask *>(task);

    counter->count();
    // count may wake and switch to another coroutine, and `*this`
    // maybe destroyed after counter->count
}

} // namespace coke
