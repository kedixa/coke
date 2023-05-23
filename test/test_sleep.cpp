#include <chrono>
#include <gtest/gtest.h>

#include "coke/coke.h"

long current_msec() {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    auto usec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
    return usec.count();
}

coke::Task<> start_thread() {
    co_await coke::sleep(std::chrono::milliseconds(0));
}

coke::Task<> do_sleep(int ms) {
    long start = current_msec();
    co_await coke::sleep(std::chrono::milliseconds(ms));
    long cost = current_msec() - start;
    // Time-cost errors are amplified when testing with valgrind
    EXPECT_NEAR(cost, ms, 10);
}

TEST(SLEEP, sleep) {
    coke::sync_wait(start_thread());
    coke::sync_wait(
        do_sleep(50),
        do_sleep(100),
        do_sleep(200)
    );
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
