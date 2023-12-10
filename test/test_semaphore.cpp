#include <atomic>
#include <vector>
#include <chrono>

#include <gtest/gtest.h>

#include "coke/coke.h"

constexpr int MAX_TASKS = 16;

enum {
    TEST_TRY_ACQUIRE = 0,
    TEST_ACQUIRE = 1,
    TEST_ACQUIRE_FOR = 2,
    TEST_ACQUIRE_UNTIL = 3,
};

struct ParamPack {
    coke::TimedSemaphore *sem;
    std::atomic<int> *x;
    int sem_max;
    int test_method;
    int loop_max;
};

coke::Task<> do_test_semaphore(ParamPack p) {
    constexpr auto ms10 = std::chrono::milliseconds(10);

    coke::TimedSemaphore &sem = *(p.sem);
    std::atomic<int> &x = *(p.x);
    int sem_max = p.sem_max;
    int test_method = p.test_method;
    int y, ret;

    for (int i = 0; i < p.loop_max; i++) {
        ret = -1;

        switch (test_method) {
        case TEST_TRY_ACQUIRE:
            if (!sem.try_acquire())
                ret = co_await sem.acquire();
            else
                ret = coke::TOP_SUCCESS;
            break;

        case TEST_ACQUIRE:
            ret = co_await sem.acquire();
            break;

        case TEST_ACQUIRE_FOR:
            ret = -1;
            while (ret != coke::TOP_SUCCESS)
                ret = co_await sem.try_acquire_for(ms10);
            break;

        case TEST_ACQUIRE_UNTIL:
            ret = -1;
            while (ret != coke::TOP_SUCCESS) {
                auto now = std::chrono::steady_clock::now();
                ret = co_await sem.try_acquire_until(now + ms10);
            }
            break;
        }

        EXPECT_EQ(ret, coke::TOP_SUCCESS);

        x.fetch_add(1, std::memory_order_relaxed);
        y = x.load(std::memory_order_relaxed);

        if (sem_max == 1)
            EXPECT_EQ(y, 1);
        else {
            EXPECT_LE(y, sem_max);
            EXPECT_GT(y, 0);
        }

        x.fetch_sub(1, std::memory_order_relaxed);
        sem.release();
    }
}

void test_semaphore(int sem_max, int test_method) {
    coke::TimedSemaphore sem(sem_max);
    std::atomic<int> x{0};

    std::vector<coke::Task<>> tasks;
    tasks.reserve(MAX_TASKS);

    ParamPack p;
    p.sem = &sem;
    p.x = &x;
    p.sem_max = sem_max;
    p.test_method = test_method;
    p.loop_max = 1024;

    for (int i = 0; i < MAX_TASKS; i++)
        tasks.emplace_back(do_test_semaphore(p));

    coke::sync_wait(std::move(tasks));
}

TEST(SEMAPHORE, sem_try_acquire) {
    test_semaphore(1, TEST_TRY_ACQUIRE);
    test_semaphore(16, TEST_TRY_ACQUIRE);
}

TEST(SEMAPHORE, sem_acquire) {
    test_semaphore(1, TEST_ACQUIRE);
    test_semaphore(16, TEST_ACQUIRE);
}

TEST(SEMAPHORE, sem_acquire_for) {
    test_semaphore(1, TEST_ACQUIRE_FOR);
    test_semaphore(16, TEST_ACQUIRE_FOR);
}

TEST(SEMAPHORE, sem_acquire_until) {
    test_semaphore(1, TEST_ACQUIRE_UNTIL);
    test_semaphore(16, TEST_ACQUIRE_UNTIL);
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 4;
    s.handler_threads = 8;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
