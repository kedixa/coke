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

#include <array>
#include <gtest/gtest.h>
#include <map>
#include <string>

#include "coke/coke.h"

struct Copy {
    Copy(const Copy &) = default;
    Copy(Copy &&) = delete;
};

struct Move {
    Move(const Move &) = delete;
    Move(Move &&) = default;
};

struct PrivateDestructor {
    PrivateDestructor() = default;
    PrivateDestructor(PrivateDestructor &&) = default;

private:
    ~PrivateDestructor() = default;
};

void func() {}

TEST(CONCEPT, cokeable)
{
    EXPECT_TRUE(coke::Cokeable<int>);
    EXPECT_TRUE(coke::Cokeable<std::string>);
    EXPECT_TRUE((coke::Cokeable<std::map<std::string, bool>>));
    EXPECT_TRUE(coke::Cokeable<Copy>);
    EXPECT_TRUE(coke::Cokeable<Move>);
    EXPECT_TRUE(coke::Cokeable<std::nullptr_t>);
    EXPECT_TRUE(coke::Cokeable<void>);
    EXPECT_TRUE(coke::Cokeable<int *>);
    EXPECT_TRUE((coke::Cokeable<std::array<int, 10>>));

    EXPECT_FALSE(coke::Cokeable<int[]>);
    EXPECT_FALSE(coke::Cokeable<int &>);
    EXPECT_FALSE(coke::Cokeable<const int>);
    EXPECT_FALSE(coke::Cokeable<int &&>);
    EXPECT_FALSE(coke::Cokeable<void (&)()>);
    EXPECT_FALSE(coke::Cokeable<PrivateDestructor>);
}

TEST(CONCEPT, co_promise)
{
    EXPECT_TRUE(coke::IsCokePromise<coke::detail::CoPromise<void>>);
    EXPECT_TRUE(coke::IsCokePromise<coke::detail::CoPromise<int>>);
    EXPECT_FALSE(coke::IsCokePromise<void>);
    EXPECT_FALSE(coke::IsCokePromise<int>);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
