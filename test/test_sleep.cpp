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

TEST(SLEEP, sleep1) {
    coke::sync_wait(start_thread());
    coke::sync_wait(
        do_sleep(50),
        do_sleep(100),
        do_sleep(200)
    );
}

coke::Task<> cancel_sleep() {
    uint64_t uid = coke::get_unique_id();
    auto awaiter = coke::sleep(uid, std::chrono::milliseconds(0));
    coke::cancel_sleep_by_id(uid);
    int ret = co_await awaiter;
    EXPECT_EQ(ret, coke::SLEEP_CANCELED);
}

coke::Task<> success_sleep() {
    uint64_t uid = coke::get_unique_id();
    auto awaiter = coke::sleep(uid, std::chrono::milliseconds(0));
    int ret = co_await awaiter;
    EXPECT_EQ(ret, coke::SLEEP_SUCCESS);
}

TEST(SLEEP, sleep2) {
    coke::sync_wait(cancel_sleep());
    coke::sync_wait(success_sleep());
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 2;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    EXPECT_GE(coke::get_unique_id(), 1);

    return RUN_ALL_TESTS();
}
