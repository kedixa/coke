#include <functional>
#include <atomic>
#include <gtest/gtest.h>

#include "coke/coke.h"
#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"

coke::Task<> generic() {
    std::atomic<int> count = 0;
    auto func = [&count]() {
        ++count;
    };

    ParallelWork *p = Workflow::create_parallel_work(nullptr);
    for (int i = 0; i < 10; i++) {
        auto *t = WFTaskFactory::create_go_task("", func);
        auto *s = Workflow::create_series_work(t, nullptr);
        p->add_series(s);
    }

    coke::GenericAwaiter<int> g;
    p->set_callback([&g](const ParallelWork *) {
        g.set_result(1234);
        g.done();
    });

    g.take_over(p);

    int ret = co_await g;
    EXPECT_EQ(ret, 1234);
    EXPECT_EQ(count.load(), 10);
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 2;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
