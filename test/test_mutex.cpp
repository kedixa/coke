#include <vector>
#include <chrono>

#include <gtest/gtest.h>

#include "coke/coke.h"

constexpr int MAX_TASKS = 16;

enum {
    TEST_TRY_LOCK = 0,
    TEST_LOCK = 1,
    TEST_LOCK_FOR = 2,
};

struct ParamPack {
    coke::TimedMutex *mtx;
    int *count;
    int test_method;
    int loop_max;
};

coke::Task<> do_test_mutex(ParamPack p) {
    constexpr auto ms10 = std::chrono::milliseconds(10);

    coke::TimedMutex &mtx = *(p.mtx);
    int &count = *(p.count);
    int test_method = p.test_method;
    int ret;

    for (int i = 0; i < p.loop_max; i++) {
        ret = -1;

        switch (test_method) {
        case TEST_TRY_LOCK:
            if (!mtx.try_lock())
                ret = co_await mtx.lock();
            else
                ret = coke::TOP_SUCCESS;
            break;

        case TEST_LOCK:
            ret = co_await mtx.lock();
            break;

        case TEST_LOCK_FOR:
            ret = -1;
            while (ret != coke::TOP_SUCCESS)
                ret = co_await mtx.try_lock_for(ms10);
            break;
        }

        EXPECT_EQ(ret, coke::TOP_SUCCESS);
        count += 1;

        mtx.unlock();
    }
}

void test_mutex(int test_method) {
    coke::TimedMutex mtx;
    int total = 0;

    std::vector<coke::Task<>> tasks;
    tasks.reserve(MAX_TASKS);

    ParamPack p;
    p.mtx = &mtx;
    p.count = &total;
    p.test_method = test_method;
    p.loop_max = 1024;

    for (int i = 0; i < MAX_TASKS; i++)
        tasks.emplace_back(do_test_mutex(p));

    coke::sync_wait(std::move(tasks));

    EXPECT_EQ(total, (MAX_TASKS * p.loop_max));
}

TEST(MUTEX, try_lock) {
    test_mutex(TEST_TRY_LOCK);
}

TEST(MUTEX, lock) {
    test_mutex(TEST_LOCK);
}

TEST(MUTEX, lock_for) {
    test_mutex(TEST_LOCK_FOR);
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 4;
    s.handler_threads = 8;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
