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

#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <gtest/gtest.h>

#include "coke/coke.h"

TEST(WAIT, lambda_empty) {
    coke::sync_call([]() -> coke::Task<> { co_return; });

    auto func = []() -> coke::Task<> { co_return; };
    coke::sync_wait(func(), func());
    coke::sync_wait(func(), func(), func());

    std::vector<coke::Task<>> tasks;
    for (size_t i = 0; i < 10; i++)
        tasks.emplace_back(func());
    coke::sync_wait(std::move(tasks));
}

TEST(WAIT, return_value) {
    auto ret1 = coke::sync_call([]() -> coke::Task<int> { co_return 1; });
    EXPECT_EQ(ret1, 1);

    auto func = []() -> coke::Task<int> { co_return 1; };
    auto ret2 = coke::sync_wait(func(), func());
    auto res2 = std::vector<int>(2, 1);
    EXPECT_EQ(ret2, res2);

    std::vector<coke::Task<int>> tasks;
    for (size_t i = 0; i < 10; i++)
        tasks.emplace_back(func());
    auto ret3 = coke::sync_wait(std::move(tasks));
    auto res3 = std::vector<int>(10, 1);
    EXPECT_EQ(ret3, res3);
}

TEST(WAIT, lambda_capture) {
    int a;

    auto ret = coke::sync_call([&]() -> coke::Task<int> {
        a = co_await coke::go(std::plus<int>{}, 1, 2);
        a += 3;
        a += co_await coke::go([](int l, int r) { return l - r; }, 1, 3);
        co_return a;
    });

    EXPECT_EQ(a, 4);
    EXPECT_EQ(ret, 4);
}

struct Func {
    Func() { }
    Func(Func &&) { }

    coke::Task<std::string> operator()(int a, int b) {
        int x = co_await coke::go(std::plus<int>{}, a, b);
        s = co_await coke::go(static_cast<std::string(*)(int)>(std::to_string), x);
        co_await coke::go(std::reverse<std::string::iterator>, s.begin(), s.end());
        co_return s;
    }

private:
    std::string s;
};

TEST(WAIT, callable_object) {
    Func f;

    std::string ret1(coke::sync_call(Func{}, 32, 14));
    std::string ret2(coke::sync_call(f, 8, 16));

    EXPECT_EQ(ret1, std::string("64"));
    EXPECT_EQ(ret2, std::string("42"));

    std::vector<std::string> ret3 = coke::sync_wait(
        coke::make_task(Func{}, 1, 1),
        coke::make_task(Func{}, 8, 5),
        coke::make_task(Func{}, 99, 24)
    );
    std::vector<std::string> res3 = {"2", "31", "321"};
    EXPECT_EQ(ret3, res3);
}

coke::Task<std::vector<int>> sorted(std::vector<int> v) {
    co_await coke::switch_go_thread();
    std::sort(v.begin(), v.end());
    co_return v;
}

TEST(WAIT, function_pointer) {
    std::vector<int> v, s;
    for (size_t i = 0; i < 64; i++)
        v.push_back(int(i * i * 201 % 97));

    s = coke::sync_call(sorted, v);

    std::sort(v.begin(), v.end());
    EXPECT_EQ(v, s);
}

coke::Task<std::string> identity(std::string s) {
    co_return s;
}

TEST(WAIT, wait_awaitable) {
    std::string s("hello");
    std::string t = coke::sync_wait(identity(s));
    EXPECT_EQ(s, t);

    coke::SleepAwaiter slp = coke::sleep(0.1);
    int ret = coke::sync_wait(std::move(slp));
    EXPECT_EQ(ret, coke::STATE_SUCCESS);
}

TEST(WAIT, wait_two_awaitable) {
    std::string s("hello");
    std::vector<std::string> ret = coke::sync_wait(
        identity(s), identity(s)
    );
    std::vector<std::string> expect{s, s};
    EXPECT_EQ(ret, expect);
}

coke::Task<> test_async() {
    std::vector<std::string> res{"asdf", "abc", "xyz"};
    auto ret = co_await coke::async_wait(
        identity(res[0]),
        identity(res[1]),
        identity(res[2])
    );

    EXPECT_EQ(ret, res);
}

TEST(WAIT, async_wait) {
    coke::sync_wait(test_async());
}

coke::Task<bool> test_bool() {
    co_await coke::yield();
    co_return true;
}

coke::Task<> async_test_bool() {
    constexpr std::size_t M = 1024;
    std::vector<bool> vec;
    std::vector<coke::Task<bool>> tasks;
    tasks.reserve(M);

    for (std::size_t i = 0; i < M; i++)
        tasks.emplace_back(test_bool());

    vec = co_await coke::async_wait(std::move(tasks));
    EXPECT_EQ(vec, std::vector<bool>(M, true));
}

TEST(WAIT, sync_vector_bool) {
    constexpr std::size_t M = 1024;
    std::vector<bool> vec;
    std::vector<coke::Task<bool>> tasks;
    tasks.reserve(M);

    for (std::size_t i = 0; i < M; i++)
        tasks.emplace_back(test_bool());

    vec = coke::sync_wait(std::move(tasks));
    EXPECT_EQ(vec, std::vector<bool>(M, true));
}

TEST(WAIT, async_vector_bool) {
    coke::sync_wait(async_test_bool());
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
