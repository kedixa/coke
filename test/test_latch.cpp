#include <string>
#include <gtest/gtest.h>
#include "coke/coke.h"

coke::Task<> simple_latch() {
    std::string s = "hello";
    std::string t;

    {
        coke::Latch lt(1);

        coke::make_task([](std::string &x, coke::Latch &lt) -> coke::Task<> {
            x = "hello";
            lt.count_down();
            co_return;
        }, t, lt).start();

        co_await lt;
        EXPECT_EQ(s, t);
    }

    {
        coke::Latch lt(1);
        t.clear();

        coke::make_task([](std::string &x, coke::Latch &lt) -> coke::Task<> {
            // switch to another thread
            co_await coke::yield();

            x = "hello";
            lt.count_down();
        }, t, lt).start();

        co_await lt;
        EXPECT_EQ(s, t);
    }
}

TEST(LATCH, simple) {
    coke::sync_wait(simple_latch());
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
