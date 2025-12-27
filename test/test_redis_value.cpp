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

#include <gtest/gtest.h>

#include "coke/redis/value.h"

TEST(REDIS_VALUE, make_value)
{
    std::string str{"hello"};

    {
        auto val = coke::make_redis_null();
        EXPECT_TRUE(val.is_null());
    }

    {
        auto val = coke::make_redis_simple_string(str);
        EXPECT_TRUE(val.is_simple_string());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_bulk_string(str);
        EXPECT_TRUE(val.is_bulk_string());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_verbatim_string(str);
        EXPECT_TRUE(val.is_verbatim_string());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_simple_error(str);
        EXPECT_TRUE(val.is_simple_error());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_bulk_error(str);
        EXPECT_TRUE(val.is_bulk_error());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_big_number(str);
        EXPECT_TRUE(val.is_big_number());
        EXPECT_EQ(val.get_string(), str);
    }

    {
        auto val = coke::make_redis_integer(42);
        EXPECT_TRUE(val.is_integer());
        EXPECT_EQ(val.get_integer(), 42);
    }

    {
        auto val = coke::make_redis_double(3.14);
        EXPECT_TRUE(val.is_double());
        EXPECT_NEAR(val.get_double(), 3.14, 1e-5);
    }

    {
        auto val = coke::make_redis_boolean(true);
        EXPECT_TRUE(val.is_boolean());
        EXPECT_TRUE(val.get_boolean());
    }

    {
        coke::RedisArray arr;
        arr.push_back(coke::make_redis_simple_string("str1"));
        arr.push_back(coke::make_redis_simple_string("str2"));

        auto val = coke::make_redis_array(std::move(arr));
        EXPECT_TRUE(val.is_array());
        EXPECT_EQ(val.array_size(), 2);
        EXPECT_EQ(val.get_array()[0].get_string(), "str1");
        EXPECT_EQ(val.get_array()[1].get_string(), "str2");
    }

    {
        coke::RedisMap map;
        coke::RedisValue key = coke::make_redis_integer(1);
        coke::RedisValue value = coke::make_redis_simple_string(str);
        map.emplace_back(coke::RedisPair{std::move(key), std::move(value)});

        auto val = coke::make_redis_map(std::move(map));
        EXPECT_TRUE(val.is_map());
        EXPECT_EQ(val.map_size(), 1);
        EXPECT_EQ(val.get_map()[0].key.get_integer(), 1);
        EXPECT_EQ(val.get_map()[0].value.get_string(), str);
    }
}

TEST(REDIS_VALUE, move_semantics)
{
    std::string str{"hello"};

    coke::RedisValue val1 = coke::make_redis_simple_string(str);
    EXPECT_TRUE(val1.is_simple_string());
    EXPECT_EQ(val1.get_string(), str);

    coke::RedisValue val2 = std::move(val1);
    EXPECT_TRUE(val2.is_simple_string());
    EXPECT_EQ(val2.get_string(), str);
}

TEST(REDIS_VALUE, copy_semantics)
{
    std::string str{"hello"};

    coke::RedisValue val1 = coke::make_redis_simple_string(str);
    EXPECT_TRUE(val1.is_simple_string());
    EXPECT_EQ(val1.get_string(), str);

    coke::RedisValue val2 = val1;
    EXPECT_TRUE(val2.is_simple_string());
    EXPECT_EQ(val2.get_string(), str);

    val1.set_simple_string("modified");
    EXPECT_EQ(val1.get_string(), "modified");
    EXPECT_EQ(val2.get_string(), str);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
