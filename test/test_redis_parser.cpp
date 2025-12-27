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

#include "coke/redis/message.h"
#include "coke/redis/parser.h"

class Response : public coke::RedisResponse {
public:
    int encode(struct iovec vectors[], int max) override
    {
        return coke::RedisResponse::encode(vectors, max);
    }
};

void test_parse(const std::string &data, int state,
                coke::RedisValue *val = nullptr)
{
    coke::RedisParser parser;
    std::size_t size = data.size();
    int ret;

    ret = parser.append(data.data(), &size);
    EXPECT_EQ(ret, state);

    if (state == 1) {
        EXPECT_EQ(size, data.size());
    }

    if (val)
        *val = std::move(parser.get_value());
}

void test_append_one_by_one(const std::string &data,
                            coke::RedisValue *val = nullptr)
{
    coke::RedisParser parser;
    std::size_t size = data.size();
    std::size_t tmp;
    int ret;

    for (std::size_t i = 0; i < size; i++) {
        tmp = 1;
        ret = parser.append(data.data() + i, &tmp);

        if (i + 1 == size)
            EXPECT_EQ(ret, 1);
        else
            EXPECT_EQ(ret, 0);

        EXPECT_EQ(tmp, 1);
    }

    EXPECT_EQ(ret, 1);

    if (val)
        *val = std::move(parser.get_value());
}

void check_value(const coke::RedisValue &val)
{
    EXPECT_TRUE(val.is_array());
    EXPECT_EQ(val.array_size(), 9);

    const coke::RedisArray &arr = val.get_array();
    EXPECT_TRUE(arr[0].is_simple_string());
    EXPECT_TRUE(arr[1].is_simple_error());
    EXPECT_TRUE(arr[2].is_integer());
    EXPECT_TRUE(arr[3].is_null());
    EXPECT_TRUE(arr[4].is_double());
    EXPECT_TRUE(arr[5].is_bulk_error());
    EXPECT_TRUE(arr[6].is_verbatim_string());
    EXPECT_TRUE(arr[7].is_big_number());
    EXPECT_TRUE(arr[8].is_set());
    EXPECT_TRUE(arr[8].has_attribute());
    EXPECT_EQ(arr[8].array_size(), 2);

    EXPECT_EQ(arr[0].get_string(), "simple string");
    EXPECT_EQ(arr[1].get_string(), "simple error");
    EXPECT_EQ(arr[2].get_integer(), 12345);
    EXPECT_NEAR(arr[4].get_double(), 3.14159, 1e-5);
    EXPECT_EQ(arr[5].get_string(), "bulk error");
    EXPECT_EQ(arr[6].get_string(), "verbatim string");
    EXPECT_EQ(arr[7].get_string(), "12345678901234567890");

    const coke::RedisArray &set = arr[8].get_array();
    EXPECT_TRUE(set[0].is_map());
    EXPECT_TRUE(set[1].is_push());
    EXPECT_EQ(set[0].map_size(), 1);
    EXPECT_EQ(set[1].array_size(), 1);

    const coke::RedisMap &map = set[0].get_map();
    const coke::RedisArray &push = set[1].get_array();

    EXPECT_TRUE(map[0].key.is_integer());
    EXPECT_TRUE(map[0].value.is_double());
    EXPECT_TRUE(push[0].is_bulk_string());

    EXPECT_EQ(map[0].key.get_integer(), 0);
    EXPECT_EQ(map[0].value.get_double(), 0.0);
    EXPECT_EQ(push[0].get_string(), "push");

    EXPECT_TRUE(arr[8].has_attribute());

    const coke::RedisMap &attr = arr[8].get_attribute();

    EXPECT_EQ(attr.size(), 1);
    EXPECT_TRUE(attr[0].key.is_boolean());
    EXPECT_TRUE(attr[0].value.is_boolean());
    EXPECT_EQ(attr[0].key.get_boolean(), true);
    EXPECT_EQ(attr[0].value.get_boolean(), false);
}

TEST(REDIS_PARSER, simple)
{
    std::string data;

    data = "*9\r\n"
           "+simple string\r\n"
           "-simple error\r\n"
           ":12345\r\n"
           "_\r\n"
           ",3.14159\r\n"
           "!10\r\nbulk error\r\n"
           "=15\r\nverbatim string\r\n"
           "(12345678901234567890\r\n"
           "|1\r\n#t\r\n#f\r\n"
           "~2\r\n%1\r\n:0\r\n,0\r\n>1\r\n$4\r\npush\r\n";

    {
        coke::RedisValue val;
        test_parse(data, 1, &val);
        check_value(val);
    }

    {
        coke::RedisValue val;
        test_append_one_by_one(data, &val);
        check_value(val);
    }
}

TEST(REDIS_PARSER, inline_command)
{
    std::string data = "mget a b c d\r\n";
    std::size_t size = data.size();

    coke::RedisParser parser;
    int ret = parser.append(data.data(), &size);

    EXPECT_EQ(ret, 1);
    EXPECT_EQ(size, data.size());

    std::vector<std::string> result = {"mget", "a", "b", "c", "d"};
    coke::RedisValue value = std::move(parser.get_value());

    EXPECT_TRUE(value.is_array());
    EXPECT_EQ(value.array_size(), result.size());

    const coke::RedisArray &array = value.get_array();
    for (std::size_t i = 0; i < array.size(); i++) {
        EXPECT_TRUE(array[i].is_bulk_string());
        EXPECT_EQ(array[i].get_string(), result[i]);
    }
}

TEST(REDIS_PARSER, memory_attack)
{
    // Heap alloc attack, declare huge array but no element.
    {
        std::string data;

        for (int i = 0; i < 4096; i++)
            data.append("*4294967295\r\n");

        test_parse(data, 0);
    }

    // Stack overflow attack, deep nesting array
    {
        std::string data;
        coke::RedisValue val;

        for (int i = 0; i < 200000; i++)
            data.append("*1\r\n");

        data.append("_\r\n");

        test_parse(data, 1, &val);
    }
}

TEST(REDIS_RESPONSE, encode)
{
    std::string data;

    data = "*9\r\n"
           "+simple string\r\n"
           "-simple error\r\n"
           ":12345\r\n"
           "_\r\n"
           ",3.14159\r\n"
           "!10\r\nbulk error\r\n"
           "=15\r\nverbatim string\r\n"
           "(12345678901234567890\r\n"
           "|1\r\n#t\r\n#f\r\n"
           "~2\r\n%1\r\n:0\r\n,0\r\n>1\r\n$4\r\npush\r\n";

    {
        coke::RedisValue val;
        test_parse(data, 1, &val);

        Response resp;
        resp.set_value(std::move(val));

        struct iovec vec;
        int ret = resp.encode(&vec, 1);
        EXPECT_EQ(ret, 1);

        std::string encoded;
        encoded.assign((const char *)vec.iov_base, vec.iov_len);

        test_parse(encoded, 1, &val);
        check_value(val);
    }
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
