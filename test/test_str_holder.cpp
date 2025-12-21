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

#include <list>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "coke/utils/str_holder.h"

using namespace std::literals;

TEST(STR_HOLDER, basic)
{
    std::string_view view("hello");

    coke::StrHolder str(std::string("hello"));
    coke::StrHolder ptr("hello");
    coke::StrHolder sv(view);

    EXPECT_TRUE(str.holds_string());
    EXPECT_TRUE(ptr.holds_string());
    EXPECT_TRUE(sv.holds_view());

    EXPECT_EQ(str.as_view(), view);
    EXPECT_EQ(ptr.as_view(), view);
    EXPECT_EQ(sv.as_view(), view);

    EXPECT_THROW(str.get_view(), std::bad_variant_access);
    EXPECT_THROW(ptr.get_view(), std::bad_variant_access);
    EXPECT_THROW(sv.get_string(), std::bad_variant_access);
}

TEST(STR_HOLDER, make_shv)
{
    std::list<std::string> lstrs{"hello", "coke", "strholder"};
    const char *ptrs[3] = {"hello", "coke", "strholder"};

    // Make shv from strs.
    auto shv = coke::make_shv("hello"s, "coke"sv, "strholder");

    EXPECT_EQ(shv.size(), 3);
    EXPECT_TRUE(shv[0].holds_string());
    EXPECT_TRUE(shv[1].holds_view());
    EXPECT_TRUE(shv[2].holds_string());

    // Make shv from array of const char *.
    shv = coke::make_shv(ptrs, ptrs + 3);

    EXPECT_EQ(shv.size(), 3);
    EXPECT_TRUE(shv[0].holds_string());
    EXPECT_TRUE(shv[1].holds_string());
    EXPECT_TRUE(shv[2].holds_string());

    EXPECT_EQ(shv[0].as_view(), "hello");
    EXPECT_EQ(shv[1].as_view(), "coke");
    EXPECT_EQ(shv[2].as_view(), "strholder");

    // Make shv from list of string.
    shv = coke::make_shv(lstrs.begin(), lstrs.end());

    EXPECT_EQ(shv.size(), 3);
    EXPECT_TRUE(shv[0].holds_string());
    EXPECT_TRUE(shv[1].holds_string());
    EXPECT_TRUE(shv[2].holds_string());

    EXPECT_EQ(shv[0].as_view(), "hello");
    EXPECT_EQ(shv[1].as_view(), "coke");
    EXPECT_EQ(shv[2].as_view(), "strholder");

    // Make shv as view from list of string.
    shv = coke::make_shv_view(lstrs.begin(), lstrs.end());

    EXPECT_EQ(shv.size(), 3);
    EXPECT_TRUE(shv[0].holds_view());
    EXPECT_TRUE(shv[1].holds_view());
    EXPECT_TRUE(shv[2].holds_view());

    EXPECT_EQ(shv[0].as_view(), "hello");
    EXPECT_EQ(shv[1].as_view(), "coke");
    EXPECT_EQ(shv[2].as_view(), "strholder");
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
