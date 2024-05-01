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

coke::Task<std::string> create_task() {
    co_await coke::sleep(milliseconds(300));
    co_return std::string(100, 'a');
}

coke::Task<void> create_void_task() {
    co_await coke::sleep(milliseconds(300));
    co_return;
}

coke::Task<> create_exception_task() {
    co_await coke::yield();
    throw std::runtime_error("this is an exception");
}

coke::Task<> test_async_future() {
    {
        std::string str(100, 'a'), result;
        coke::Future<std::string> fut = coke::create_future(create_task());
        int ret;

        do {
            ret = co_await fut.wait_for(milliseconds(100));
        } while (ret != coke::FUTURE_STATE_READY);

        EXPECT_TRUE(fut.valid());
        EXPECT_TRUE(fut.ready());
        EXPECT_FALSE(fut.broken());
        result = fut.get();
        EXPECT_EQ(str, result);
    }

    {
        coke::Future<void> fut = coke::create_future(create_void_task());
        int ret, n = 0;

        do {
            ret = co_await fut.wait_for(milliseconds(100));
            ++n;
        } while (ret != coke::FUTURE_STATE_READY);

        EXPECT_GE(n, 1);
        EXPECT_LE(n, 5);
        EXPECT_TRUE(fut.valid());
        EXPECT_TRUE(fut.ready());
        EXPECT_FALSE(fut.broken());
        fut.get();
    }

    {
        coke::Future<void> fut = coke::create_future(create_exception_task());

        int ret;
        bool has_exception = false;

        do {
            ret = co_await fut.wait_for(milliseconds(100));
        } while (ret == coke::FUTURE_STATE_NOTSET);

        EXPECT_EQ(ret, coke::FUTURE_STATE_EXCEPTION);
        EXPECT_TRUE(fut.has_exception());

        try {
            fut.get();
        }
        catch (const std::runtime_error &) {
            has_exception = true;
        }

        EXPECT_TRUE(has_exception);
    }
}

template<typename T>
coke::Task<> do_test(T data, int ms, int state) {
    coke::Promise<T> pro;
    coke::Future<T> fut = pro.get_future();
    int wait_state;

    process<T>(data, std::move(pro)).start();

    wait_state = co_await fut.wait_for(milliseconds(ms));
    EXPECT_EQ(wait_state, state);

    co_await fut.wait();
}

coke::Task<> do_test(int ms, int state) {
    coke::Promise<void> pro;
    coke::Future<void> fut = pro.get_future();
    int wait_state;

    process(std::move(pro)).start();

    wait_state = co_await fut.wait_for(milliseconds(ms));
    EXPECT_EQ(wait_state, state);

    co_await fut.wait();
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

TEST(FUTURE, broken) {
    coke::Future<int> f;
    EXPECT_FALSE(f.valid());

    {
        coke::Promise<int> p;
        f = p.get_future();
        EXPECT_TRUE(f.valid());
    }

    EXPECT_TRUE(f.broken());

    int ret = coke::sync_wait(f.wait());
    EXPECT_EQ(ret, coke::FUTURE_STATE_BROKEN);
}

TEST(FUTURE, from_task) {
    coke::sync_wait(test_async_future());
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
