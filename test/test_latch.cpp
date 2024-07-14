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
        }, t, lt).detach();

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
        }, t, lt).detach();

        co_await lt;
        EXPECT_EQ(s, t);
    }
}

coke::Task<> ret_value() {
    coke::Latch lt(1);
    int ret;

    auto func = [&]() -> coke::Task<> {
        co_await coke::sleep(0.2);
        lt.count_down();
    };

    func().detach();

    ret = co_await lt.wait_for(std::chrono::milliseconds(10));
    EXPECT_EQ(ret, coke::LATCH_TIMEOUT);

    ret = co_await lt;
    EXPECT_EQ(ret, coke::LATCH_SUCCESS);
}

TEST(LATCH, simple) {
    coke::sync_wait(simple_latch());
}

TEST(LATCH, ret_value) {
    coke::sync_wait(ret_value());
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
