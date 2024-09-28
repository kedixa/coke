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
#include <vector>
#include <gtest/gtest.h>

#include "coke/coke.h"

constexpr int MAX_TASKS = 32;
constexpr auto ms10 = std::chrono::milliseconds(10);
constexpr auto ms1 = std::chrono::milliseconds(1);
constexpr auto us1 = std::chrono::microseconds(1);
constexpr auto relaxed = std::memory_order_relaxed;

enum {
    TEST_TRY_LOCK = 0,
    TEST_LOCK = 1,
    TEST_LOCK_FOR = 2,
    TEST_TRY_LOCK_SHARED = 3,
    TEST_LOCK_SHARED = 4,
    TEST_TRY_LOCK_SHARED_FOR = 5,
};

struct ParamPack {
    coke::SharedMutex mtx;
    std::atomic<int> count;
    std::atomic<int> total;
    int test_method;
    int loop_max;
};

coke::Task<> do_test_mutex(ParamPack *p) {
    coke::SharedMutex &mtx = p->mtx;
    bool shared = (p->test_method >= TEST_TRY_LOCK_SHARED);
    int ret;

    for (int i = 0; i < p->loop_max; i++) {
        ret = -1;

        switch (p->test_method) {
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
            while (ret != coke::TOP_SUCCESS)
                ret = co_await mtx.try_lock_for(ms10);
            break;

        case TEST_TRY_LOCK_SHARED:
            if (!mtx.try_lock_shared())
                ret = co_await mtx.lock_shared();
            else
                ret = coke::TOP_SUCCESS;
            break;

        case TEST_LOCK_SHARED:
            ret = co_await mtx.lock_shared();
            break;

        case TEST_TRY_LOCK_SHARED_FOR:
            while (ret != coke::TOP_SUCCESS)
                ret = co_await mtx.try_lock_shared_for(ms10);
            break;
        }

        EXPECT_EQ(ret, coke::TOP_SUCCESS);
        p->total.fetch_add(1, relaxed);
        ret = 1 + p->count.fetch_add(1, relaxed);

        if (shared)
            co_await coke::sleep(ms1);
        else
            co_await coke::sleep(us1);

        if (shared)
            EXPECT_TRUE(ret > 0 && ret <= MAX_TASKS);
        else
            EXPECT_EQ(ret, 1);

        p->count.fetch_sub(1, relaxed);
        mtx.unlock();
    }
}

void test_mutex(int test_method) {
    ParamPack p;
    p.count = 0;
    p.total = 0;
    p.test_method = test_method;
    p.loop_max = 128;

    std::vector<coke::Task<>> tasks;
    tasks.reserve(MAX_TASKS);

    for (int i = 0; i < MAX_TASKS; i++)
        tasks.emplace_back(do_test_mutex(&p));

    coke::sync_wait(std::move(tasks));

    EXPECT_EQ(p.total.load(), (MAX_TASKS * p.loop_max));
}

TEST(SHARED_MUTEX, try_lock) {
    test_mutex(TEST_TRY_LOCK);
}

TEST(SHARED_MUTEX, lock) {
    test_mutex(TEST_LOCK);
}

TEST(SHARED_MUTEX, lock_for) {
    test_mutex(TEST_LOCK_FOR);
}

TEST(SHARED_MUTEX, try_lock_shared) {
    test_mutex(TEST_TRY_LOCK_SHARED);
}

TEST(SHARED_MUTEX, lock_shared) {
    test_mutex(TEST_LOCK_SHARED);
}

TEST(SHARED_MUTEX, lock_shared_for) {
    test_mutex(TEST_TRY_LOCK_SHARED_FOR);
}

TEST(SHARED_MUTEX, shared_and_unique) {
    int loop_max = 128;
    std::atomic<bool> finish{false};
    std::atomic<int> shared_count{0};
    coke::SharedMutex mtx;

    auto lock_shared = [&]() -> coke::Task<> {
        while (!finish.load()) {
            co_await mtx.lock_shared();
            shared_count.fetch_add(1, relaxed);
            co_await coke::sleep(ms1);
            shared_count.fetch_sub(1, relaxed);
            mtx.unlock();
        }
    };

    auto lock_unique = [&]() -> coke::Task<> {
        for (int i = 0; i < loop_max; i++) {
            co_await mtx.lock();
            EXPECT_EQ(shared_count.load(relaxed), 0);
            co_await coke::sleep(ms1);
            mtx.unlock();
        }

        finish.store(true);
    };

    coke::sync_wait(
        lock_shared(),
        lock_shared(),
        lock_shared(),
        lock_shared(),
        lock_unique()
    );
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 4;
    s.handler_threads = 8;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
