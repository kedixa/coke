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
#include <gtest/gtest.h>
#include <thread>

#include "coke/coke.h"

std::atomic<int> sleep_cnt{0};

coke::Task<> sleep(coke::WaitGroup &wg)
{
    co_await coke::sleep(0.05);
    sleep_cnt.fetch_add(1);
    wg.done();
}

coke::Task<> zero_count()
{
    coke::WaitGroup wg;
    auto id1 = std::this_thread::get_id();
    int ret;

    // Will return immediately
    ret = co_await wg.wait();
    EXPECT_EQ(ret, coke::WAIT_GROUP_SUCCESS);

    auto id2 = std::this_thread::get_id();
    EXPECT_EQ(id1, id2);
}

coke::Task<> normal_use()
{
    coke::WaitGroup wg;
    int ret;

    sleep_cnt.store(0);

    for (int i = 0; i < 10; i++) {
        wg.add(1);
        coke::detach(sleep(wg));
    }

    ret = co_await wg.wait();
    EXPECT_EQ(sleep_cnt.load(), 10);
    EXPECT_EQ(ret, coke::WAIT_GROUP_SUCCESS);
}

coke::Task<> multiple_use()
{
    coke::WaitGroup wg;
    int ret;

    wg.add(1);
    wg.done();

    ret = co_await wg.wait();
    EXPECT_EQ(ret, coke::WAIT_GROUP_SUCCESS);

    sleep_cnt.store(0);
    wg.add(1);
    coke::detach(sleep(wg));

    ret = co_await wg.wait();
    EXPECT_EQ(sleep_cnt.load(), 1);
    EXPECT_EQ(ret, coke::WAIT_GROUP_SUCCESS);
}

TEST(WAIT_GROUP, zero_count)
{
    coke::sync_wait(zero_count());
}

TEST(WAIT_GROUP, normal_use)
{
    coke::sync_wait(normal_use());
}

TEST(WAIT_GROUP, multiple_use)
{
    coke::sync_wait(multiple_use());
}

int main(int argc, char *argv[])
{
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 2;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
