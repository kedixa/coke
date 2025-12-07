/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <vector>

#include "coke/coke.h"

constexpr int MAX_TASKS = 16;
constexpr auto ms10 = std::chrono::milliseconds(10);
constexpr auto us1 = std::chrono::microseconds(1);
constexpr auto relaxed = std::memory_order_relaxed;

enum {
    TEST_TRY_ACQUIRE = 0,
    TEST_ACQUIRE = 1,
    TEST_ACQUIRE_FOR = 2,
};

struct ParamPack {
    coke::Semaphore sem;
    std::atomic<int> count;
    std::atomic<int> total;
    int sem_max;
    int test_method;
    int loop_max;
};

coke::Task<> do_test_semaphore(ParamPack *p)
{
    coke::Semaphore &sem = p->sem;
    int ret;

    for (int i = 0; i < p->loop_max; i++) {
        ret = -1;

        switch (p->test_method) {
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
            while (ret != coke::TOP_SUCCESS)
                ret = co_await sem.try_acquire_for(ms10);
            break;
        }

        EXPECT_EQ(ret, coke::TOP_SUCCESS);
        p->total.fetch_add(1, relaxed);
        ret = 1 + p->count.fetch_add(1, relaxed);

        co_await coke::sleep(us1);
        EXPECT_TRUE(ret > 0 && ret <= p->sem_max);

        p->count.fetch_sub(1, relaxed);
        sem.release();
    }
}

void test_semaphore(int sem_max, int test_method)
{
    ParamPack p{.sem = coke::Semaphore(sem_max),
                .count = 0,
                .total = 0,
                .sem_max = sem_max,
                .test_method = test_method,
                .loop_max = 128};

    std::vector<coke::Task<>> tasks;
    tasks.reserve(MAX_TASKS);

    for (int i = 0; i < MAX_TASKS; i++)
        tasks.emplace_back(do_test_semaphore(&p));

    coke::sync_wait(std::move(tasks));

    EXPECT_EQ(p.total.load(), (MAX_TASKS * p.loop_max));
}

TEST(SEMAPHORE, sem_try_acquire)
{
    test_semaphore(1, TEST_TRY_ACQUIRE);
    test_semaphore(16, TEST_TRY_ACQUIRE);
}

TEST(SEMAPHORE, sem_acquire)
{
    test_semaphore(1, TEST_ACQUIRE);
    test_semaphore(16, TEST_ACQUIRE);
}

TEST(SEMAPHORE, sem_acquire_for)
{
    test_semaphore(1, TEST_ACQUIRE_FOR);
    test_semaphore(16, TEST_ACQUIRE_FOR);
}

int main(int argc, char *argv[])
{
    coke::GlobalSettings s;
    s.poller_threads = 4;
    s.handler_threads = 8;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
