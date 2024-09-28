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

#include <vector>
#include <gtest/gtest.h>

#include "coke/coke.h"

using Milli = std::chrono::milliseconds;

std::mutex mtx;
coke::Condition cv;
int x, y;

coke::Task<int> cv_wait() {
    int ret;

    std::unique_lock<std::mutex> lk(mtx);
    ret = co_await cv.wait(lk, []() { return x == y; });
    EXPECT_EQ(ret, coke::TOP_SUCCESS);

    co_return x;
}

coke::Task<> test_wait() {
    x = 0;
    y = 1;

    auto fut = coke::create_future(cv_wait());

    co_await coke::sleep(Milli(100));
    cv.notify_all();
    co_await coke::sleep(Milli(100));

    {
        std::lock_guard<std::mutex> lk(mtx);
        x = y;
    }

    cv.notify_all();

    co_await fut.wait();
    EXPECT_EQ(fut.get(), y);
}

coke::Task<int> cv_wait_for(std::chrono::nanoseconds ns) {
    int ret;

    std::unique_lock<std::mutex> lk(mtx);

    do {
        ret = co_await cv.wait_for(lk, ns, []() { return x == y; });

        if (x == y)
            EXPECT_EQ(ret, coke::TOP_SUCCESS);
        else
            EXPECT_EQ(ret, coke::TOP_TIMEOUT);
    } while (ret != coke::TOP_SUCCESS);

    co_return x;
}

coke::Task<> test_wait_for() {
    x = 0;
    y = 1;

    auto fut = coke::create_future(cv_wait_for(Milli(20)));

    co_await coke::sleep(Milli(100));
    cv.notify_all();
    co_await coke::sleep(Milli(100));

    {
        std::lock_guard<std::mutex> lk(mtx);
        x = y;
    }

    cv.notify_all();

    co_await fut.wait();
    EXPECT_EQ(fut.get(), y);
}

TEST(CONDITION, wait) {
    coke::sync_wait(test_wait());
}

TEST(CONDITION, wait_for) {
    coke::sync_wait(test_wait_for());
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
