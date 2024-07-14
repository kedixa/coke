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
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>

#include "coke/tools/option_parser.h"

struct Config {
    bool flag;
    bool boolean{true};
    int verbose;
    int16_t i16;
    uint32_t u32;
    uint64_t data_unit;
    float f32 = 0.0f;
    std::string str;
    std::vector<double> vf64;
};

TEST(OPTION_PARSER, basic) {
    std::vector<std::string> argv {
        "program_name", "-f", "--flag", "-vvv", "--boolean=no",
        "-i", "-1234", "-i", "-4321", "--uint32", "5678",
        "-d=1E2p3TB4Gb56mB78kb90",
        "-y=-0.1", "-y", "0.2", "--vf64=1e123",
        "-s", "this is a string", "--", "ex1", "--ex2", "-ex3"
    };

    coke::OptionParser args;
    Config cfg;

    args.add_flag(cfg.flag, 'f', "flag");
    args.add_countable_flag(cfg.verbose, 'v', "verbose");
    args.add_bool(cfg.boolean, coke::NULL_SHORT_NAME, "boolean");
    args.add_integer(cfg.i16, 'i', coke::NULL_LONG_NAME);
    args.add_integer(cfg.u32, 'u', "uint32", true, "desc");
    args.add_data_unit(cfg.data_unit, 'd', "data-unit");
    args.add_floating(cfg.f32, 'x', "f32").set_default(3.14f);
    args.add_multi_floating(cfg.vf64, 'y', "vf64");
    args.add_string(cfg.str, 's', "string");
    args.set_help_flag('h', "help");

    int ret = args.parse(argv);
    std::string str("this is a string");
    std::vector<std::string> ex1{"ex1", "--ex2", "-ex3"};
    std::vector<std::string> ex2 = args.get_extra_args();

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(cfg.flag, true);
    EXPECT_EQ(cfg.verbose, 3);
    EXPECT_EQ(cfg.boolean, false);
    EXPECT_EQ(cfg.i16, -4321);
    EXPECT_EQ(cfg.u32, 5678);
    EXPECT_EQ(cfg.data_unit, 1155176607309183066ULL);
    EXPECT_FLOAT_EQ(cfg.f32, 3.14f);
    EXPECT_EQ(cfg.str, str);
    EXPECT_EQ(ex1, ex2);

    EXPECT_EQ(cfg.vf64.size(), 3);
    EXPECT_DOUBLE_EQ(cfg.vf64[0], -0.1);
    EXPECT_DOUBLE_EQ(cfg.vf64[1], 0.2);
    EXPECT_DOUBLE_EQ(cfg.vf64[2], 1e123);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
