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
#include <random>
#include <string>
#include <vector>

#include "coke/utils/str_packer.h"

std::random_device rd;

void test_merge(const std::vector<std::string> &vstr,
                const std::string_view &result, std::size_t max)
{
    coke::StrPacker pack;

    for (std::size_t i = 0; i < vstr.size(); i++) {
        if (i % 2 == 0)
            pack.append(vstr[i]);
        else
            pack.append_nocopy(vstr[i]);
    }

    std::size_t strs_cnt = pack.strs_count();
    EXPECT_LE(strs_cnt, vstr.size());

    pack.merge(max);
    EXPECT_LE(pack.strs_count(), max);
    EXPECT_LE(pack.strs_count(), strs_cnt);

    pack.merge(1);
    EXPECT_EQ(pack.strs_count(), 1);

    const auto &strs = pack.get_strs();
    EXPECT_EQ(strs[0].as_view(), result);
}

TEST(STR_PACKER, merge)
{
    constexpr std::size_t N = 200;

    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dis;
    std::vector<std::string> vstr;
    std::string result;
    std::size_t total_len = 0;

    vstr.reserve(N);

    for (std::size_t i = 0; i < N; i++) {
        double d = std::pow(2.0, dis(mt) * 12);
        std::size_t len = (std::size_t)d;
        std::string tmp;

        tmp.reserve(len);
        for (std::size_t j = 0; j < len; j++)
            tmp.push_back(mt() % 26 + 'a');

        vstr.push_back(std::move(tmp));
        total_len += len;
    }

    result.reserve(total_len);
    for (std::size_t i = 0; i < N; i++) {
        result.append(vstr[i]);
    }

    for (std::size_t i = 1; i < N + 10; i += 7)
        test_merge(vstr, result, i);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
