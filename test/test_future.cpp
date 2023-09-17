#include <chrono>
#include <string>
#include <gtest/gtest.h>

#include "coke/coke.h"
#include "coke/future.h"
using std::chrono::milliseconds;

template<typename T>
coke::Task<> process(T data, coke::Promise<T> p) {
    co_await coke::sleep(milliseconds(300));
    p.set_value(data);
}

coke::Task<> process(coke::Promise<void> p) {
    co_await coke::sleep(milliseconds(300));
    p.set_value();
}

template<typename T>
coke::Task<> do_test(T data, int ms, int state) {
    coke::Promise<T> pro;
    coke::Future<T> fut = pro.get_future();
    int wait_state;

    process<T>(data, std::move(pro)).start();

    wait_state = co_await fut.wait_for(milliseconds(ms));
    EXPECT_EQ(wait_state, state);

    fut.wait();
}

coke::Task<> do_test(int ms, int state) {
    coke::Promise<void> pro;
    coke::Future<void> fut = pro.get_future();
    int wait_state;

    process(std::move(pro)).start();

    wait_state = co_await fut.wait_for(milliseconds(ms));
    EXPECT_EQ(wait_state, state);

    fut.wait();
}

TEST(FUTURE, string) {
    do_test(std::string(120, 'a'), 200, coke::FUTURE_STATE_TIMEOUT);
    do_test(std::string(120, 'a'), 400, coke::FUTURE_STATE_READY);
}

TEST(FUTURE, integer) {
    do_test(1, 200, coke::FUTURE_STATE_TIMEOUT);
    do_test(1, 400, coke::FUTURE_STATE_READY);
}

TEST(FUTURE, void) {
    do_test(200, coke::FUTURE_STATE_TIMEOUT);
    do_test(400, coke::FUTURE_STATE_READY);
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
