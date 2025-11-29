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

#include <gtest/gtest.h>
#include <vector>

#include "coke/coke.h"

void simple() {}
int add(int a, int b)
{
    return a + b;
}

template<typename T>
T ref(T &a, T b)
{
    a = b;
    return b;
}

template<typename T>
void swap(T &a, T &b)
{
    std::string c(std::move(a));
    a = std::move(b);
    b = std::move(c);
}

struct Callable {
    Callable() {}
    Callable(const std::string &s) : s(s) {}

    int operator()() & { return 1; }
    int operator()() && { return 2; }

    std::string call(const std::string &t) { return s + t; }

    std::string s;
};

coke::Task<> test_more()
{
    Callable callable;

    int ret1 = co_await coke::go(callable);
    int ret2 = co_await coke::go(Callable{});

    // always call as lvalue
    EXPECT_EQ(ret1, 1);
    EXPECT_EQ(ret2, 1);

    Callable callable2(std::string(50, 's'));
    auto task = coke::go(&Callable::call, &callable2, std::string(30, 's'));

    {
        auto task2 = std::move(task);
        // std::move is not necessary, here is to avoid the bug of gcc 11
        std::string str = co_await std::move(task2);

        EXPECT_EQ(str, std::string(80, 's'));
    }

    std::string a(100, 'a');
    std::string b(100, 'b');

    co_await coke::go(swap<std::string>, std::ref(a), std::ref(b));

    EXPECT_EQ(a, std::string(100, 'b'));
    EXPECT_EQ(b, std::string(100, 'a'));

    co_await coke::switch_go_thread("name");

    EXPECT_EQ(a, std::string(100, 'b'));
    EXPECT_EQ(b, std::string(100, 'a'));
}

TEST(GO, simple)
{
    coke::sync_wait(coke::go(simple), coke::go("queue", simple));
}

TEST(GO, add)
{
    int ret = coke::sync_wait(coke::go(add, 1, 2));
    EXPECT_EQ(ret, 3);
}

TEST(GO, ref)
{
    std::vector<int> a;
    std::vector<int> b{1, 2, 3, 4};
    coke::sync_wait(coke::go(ref<std::vector<int>>, std::ref(a), b));
    EXPECT_EQ(a, b);
}

TEST(GO, more)
{
    coke::sync_wait(test_more());
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
