#include <gtest/gtest.h>

#include "workflow/WFTaskFactory.h"
#include "coke/coke.h"

coke::Task<> test_parallel() {
    constexpr int N = 10;
    ParallelWork *par = Workflow::create_parallel_work(nullptr);

    for (int i = 0; i < N; i++) {
        WFTimerTask *t = WFTaskFactory::create_timer_task(0, 0, nullptr);
        SeriesWork *series = Workflow::create_series_work(t, nullptr);
        series->push_back(WFTaskFactory::create_counter_task(0, nullptr));
        series->push_back(WFTaskFactory::create_go_task("", []{}));
        series->set_context((void *)(intptr_t)i);
        par->add_series(series);
    }

    const ParallelWork *ret = co_await coke::wait_parallel(par);

    EXPECT_EQ(ret, par);
    EXPECT_EQ(par->size(), N);

    for (int i = 0; i < N; i++) {
        SeriesWork *series = par->series_at(i);
        EXPECT_EQ(series->get_context(), (void *)(intptr_t)i);
    }
}

TEST(PARALLEL, task) {
    coke::sync_wait(test_parallel());
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
