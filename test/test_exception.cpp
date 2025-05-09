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

template<typename T>
coke::Task<T> func(bool e, T x) {
    co_await coke::yield();

    if (e)
        throw x;

    co_return x;
}

template<typename T>
coke::Task<> task_exception(bool e, coke::Task<T> task, const T &x) {
    bool has_exception = false;
    T y;

    try {
        y = co_await std::move(task);
    }
    catch (const T &exc) {
        y = exc;
        has_exception =  true;
    }

    EXPECT_EQ(x, y);
    EXPECT_EQ(has_exception, e);
}

template<typename T>
coke::Task<> future_exception(bool e, coke::Future<T> fut, const T &x) {
    co_await fut.wait();

    if (e) {
        EXPECT_TRUE(fut.has_exception());
    }
    else {
        T y = fut.get();
        EXPECT_EQ(x, y);
    }
}

TEST(EXCEPTION, task) {
    coke::sync_wait(
        task_exception<int>(true, func(true, 1), 1)
    );
    coke::sync_wait(
        task_exception<int>(false, func(false, 1), 1)
    );

    std::string s(100, 'a');

    coke::sync_wait(
        task_exception<std::string>(true, func(true, s), s)
    );
    coke::sync_wait(
        task_exception<std::string>(false, func(false, s), s)
    );
}

TEST(EXCEPTION, future) {
    coke::sync_wait(
        future_exception<int>(true, coke::create_future(func(true, 1)), 1)
    );
    coke::sync_wait(
        future_exception<int>(false, coke::create_future(func(false, 1)), 1)
    );

    std::string s(100, 'a');

    coke::sync_wait(
        future_exception<std::string>(true, coke::create_future(func(true, s)), s)
    );
    coke::sync_wait(
        future_exception<std::string>(false, coke::create_future(func(false, s)), s)
    );
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
