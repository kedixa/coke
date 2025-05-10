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

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "coke/coke.h"
#include "coke/nspolicy/weighted_least_conn_policy.h"
#include "coke/nspolicy/weighted_random_policy.h"
#include "coke/nspolicy/weighted_round_robin_policy.h"

void test_policy(coke::NSPolicy *policy)
{
    std::string host1 = "host1", host2 = "host2";
    std::string port1 = "1", port2 = "2";
    coke::AddressParams params;
    coke::AddressInfo *addr1, *addr2;
    ParsedURI uri;

    // add
    EXPECT_EQ(policy->address_count(), 0);
    EXPECT_EQ(policy->available_address_count(), 0);
    EXPECT_FALSE(policy->has_address(host1, port1));
    EXPECT_EQ(policy->get_address(host1, port1), nullptr);

    policy->add_address(host1, port1, params);

    EXPECT_EQ(policy->address_count(), 1);
    EXPECT_EQ(policy->available_address_count(), 1);
    EXPECT_TRUE(policy->has_address(host1, port1));

    addr1 = policy->get_address(host1, port1);
    EXPECT_EQ(addr1->get_host(), host1);
    EXPECT_EQ(addr1->get_port(), port1);
    EXPECT_EQ(addr1->get_state(), coke::ADDR_STATE_GOOD);
    addr1->dec_ref();

    // select
    addr1 = policy->select_address(uri, {});
    addr2 = policy->select_address(uri, {addr1});
    EXPECT_NE(addr1, nullptr);
    EXPECT_EQ(addr1, addr2);
    addr1->dec_ref();
    addr2->dec_ref();

    // break
    policy->break_address(host1, port1);
    EXPECT_EQ(policy->address_count(), 1);
    EXPECT_EQ(policy->available_address_count(), 0);

    addr1 = policy->get_address(host1, port1);
    EXPECT_EQ(addr1->get_state(), coke::ADDR_STATE_DISABLED);
    addr1->dec_ref();

    addr1 = policy->select_address(uri, {});
    EXPECT_EQ(addr1, nullptr);

    // recover
    policy->recover_address(host1, port1);
    EXPECT_EQ(policy->address_count(), 1);
    EXPECT_EQ(policy->available_address_count(), 1);
    EXPECT_EQ(policy->has_address(host1, port1), true);

    addr1 = policy->get_address(host1, port1);
    EXPECT_EQ(addr1->get_state(), coke::ADDR_STATE_GOOD);
    addr1->dec_ref();

    // add
    policy->add_address(host2, port2, params);
    EXPECT_EQ(policy->address_count(), 2);
    EXPECT_EQ(policy->available_address_count(), 2);
    EXPECT_EQ(policy->has_address(host2, port2), true);

    auto addrs = policy->get_all_address();
    EXPECT_EQ(addrs.size(), 2);

    // addr disable
    addr1 = policy->select_address(uri, {});
    policy->addr_failed(addr1);
    EXPECT_EQ(addr1->get_state(), coke::ADDR_STATE_DISABLED);

    EXPECT_EQ(policy->address_count(), 2);
    EXPECT_EQ(policy->available_address_count(), 1);
    policy->recover_address(addr1->get_host(), addr1->get_port());
    addr1->dec_ref();

    // remove
    policy->remove_address(host1, port1);
    EXPECT_EQ(policy->address_count(), 1);
    EXPECT_EQ(policy->available_address_count(), 1);
    EXPECT_EQ(policy->has_address(host1, port1), false);
    EXPECT_EQ(policy->has_address(host2, port2), true);

    policy->remove_address(host2, port2);
    EXPECT_EQ(policy->address_count(), 0);
}

TEST(NSPOLICY, basic)
{
    coke::NSPolicyParams params;
    params.max_fail_marks = 1;

    coke::WeightedRandomPolicy wr(params);
    coke::WeightedRoundRobinPolicy wrr(params);
    coke::WeightedLeastConnPolicy wlc(params);

    test_policy(&wr);
    test_policy(&wrr);
    test_policy(&wlc);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
