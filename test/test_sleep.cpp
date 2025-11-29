/**
 * Copyright 2024-2025 Coke Project (https://github.com/kedixa/coke)
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

#include <chrono>
#include <gtest/gtest.h>

#include "coke/coke.h"

coke::Task<> do_sleep(int ms)
{
    co_await coke::sleep(std::chrono::milliseconds(ms));
}

TEST(SLEEP, sleep1)
{
    coke::sync_wait(do_sleep(50), do_sleep(100), do_sleep(200));
}

coke::Task<> cancel_sleep()
{
    uint64_t uid = coke::get_unique_id();
    auto awaiter = coke::sleep(uid, std::chrono::milliseconds(0));
    coke::cancel_sleep_by_id(uid);

    int ret = co_await std::move(awaiter);
    EXPECT_EQ(ret, coke::SLEEP_CANCELED);
}

coke::Task<> success_sleep()
{
    uint64_t uid = coke::get_unique_id();
    auto awaiter = coke::sleep(uid, std::chrono::milliseconds(0));
    int ret = co_await std::move(awaiter);
    EXPECT_EQ(ret, coke::SLEEP_SUCCESS);
}

TEST(SLEEP, sleep2)
{
    coke::sync_wait(cancel_sleep());
    coke::sync_wait(success_sleep());
}

int main(int argc, char *argv[])
{
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 2;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    EXPECT_GE(coke::get_unique_id(), 1);

    return RUN_ALL_TESTS();
}
