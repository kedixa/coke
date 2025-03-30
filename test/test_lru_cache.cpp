/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
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
#include <string>
#include <string_view>
#include <gtest/gtest.h>

#include "coke/lru_cache.h"
#include "coke/sleep.h"
#include "coke/wait.h"

using IntCache = coke::LruCache<int, int>;
using IntHandle = IntCache::Handle;

using StrCache = coke::LruCache<std::string, std::string>;
using StrHandle = StrCache::Handle;

coke::Task<void> test_wait(IntCache &cache, int key, int value) {
    co_await coke::yield();

    auto [h, is_created] = cache.get_or_create(key);
    EXPECT_TRUE(h);

    if (is_created) {
        EXPECT_TRUE(h.waiting());
        EXPECT_FALSE(h.success());
        co_await coke::sleep(std::chrono::milliseconds(10));
        h.emplace_value(value);
        h.notify_all();
    }
    else {
        co_await h.wait();
    }

    EXPECT_TRUE(h.success());
    EXPECT_EQ(h.key(), key);
    EXPECT_EQ(h.value(), value);
}

TEST(LRU_CACHE, basic) {
    StrCache cache(10);

    StrHandle h = cache.get("hello");
    EXPECT_FALSE(h);

    h = cache.put("hello", "world");
    EXPECT_TRUE(h);
    EXPECT_TRUE(h.success());
    EXPECT_EQ(h.key(), "hello");
    EXPECT_EQ(h.value(), "world");

    bool is_created;

    std::tie(h, is_created) = cache.get_or_create("hello");
    EXPECT_FALSE(is_created);
    EXPECT_TRUE(h);

    cache.remove(h);

    std::tie(h, is_created) = cache.get_or_create("hello");
    EXPECT_TRUE(is_created);
    EXPECT_FALSE(h.success());

    h.emplace_value("world");
    EXPECT_TRUE(h.success());

    h = cache.get(std::string_view("hello"));
    EXPECT_TRUE(h);
    EXPECT_TRUE(h.success());

    cache.remove(h);
    EXPECT_TRUE(h);
    EXPECT_TRUE(h.success());
}

TEST(LRU_CACHE, create_value) {
    StrCache cache(10);
    auto [h, is_created] = cache.get_or_create("key");

    EXPECT_TRUE(is_created);
    EXPECT_TRUE(h);
    EXPECT_TRUE(h.waiting());

    h.create_value([](std::optional<std::string> &value) {
        value.emplace(10, 'a');
    });
    h.notify_all();
    
    EXPECT_TRUE(h.success());
    EXPECT_EQ(h.value(), std::string(10, 'a'));
}

TEST(LRU_CACHE, failed) {
    StrCache cache(10);
    auto [h, is_created] = cache.get_or_create("key");

    EXPECT_TRUE(is_created);
    EXPECT_TRUE(h);
    EXPECT_TRUE(h.waiting());

    h.set_failed();
    
    EXPECT_TRUE(h.failed());
    EXPECT_FALSE(h.waiting());
}

TEST(LRU_CACHE, max_size) {
    IntCache cache(10);

    for (int i = 0; i < 11; i++)
        cache.put(i, i);

    IntHandle h1 = cache.get(0);
    IntHandle h2 = cache.get(1);

    EXPECT_FALSE(h1);
    EXPECT_TRUE(h2);
    EXPECT_EQ(cache.size(), 10);
}

TEST(LRU_CACHE, wait) {
    IntCache cache(10);

    coke::sync_wait(
        test_wait(cache, 1, 2),
        test_wait(cache, 1, 2),
        test_wait(cache, 1, 2)
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
