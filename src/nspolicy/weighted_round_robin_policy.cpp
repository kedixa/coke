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

#include "coke/nspolicy/weighted_round_robin_policy.h"

#include "coke/detail/random.h"
#include "coke/nspolicy/weighted_policy_base_impl.h"

namespace coke {

template class WeightedPolicyBase<WeightedRoundRobinPolicy>;

using AddrInfo = WeightedRoundRobinAddressInfo;

void WeightedRoundRobinPolicy::add_to_policy(AddrInfo *addr)
{
    uint64_t rnd = rand_u64();
    uint64_t offset = rnd % (SEATINGS / addr->get_weight());

    addr->reset_offset(cur_offset + offset);
    policy_set.insert(addr);
    available_weight += addr->get_weight();
}

void WeightedRoundRobinPolicy::remove_from_policy(AddrInfo *addr)
{
    policy_set.erase(addr);
    available_weight -= addr->get_weight();
}

AddrInfo *WeightedRoundRobinPolicy::select_addr(const ParsedURI &uri,
                                                const SelectHistory &history)
{
    if (available_weight == 0)
        return nullptr;

    // select fail when available weight is too small
    auto min_percent = params.min_available_percent;
    if (min_percent > 0 && available_weight < total_weight) {
        uint64_t m = total_weight * min_percent;
        if (available_weight * 100 < m)
            return nullptr;
    }

    AddrInfo *prev = nullptr;
    if (!history.empty())
        prev = addr_cast(history.back());

    auto it = policy_set.lower_bound(cur_offset);
    if (it == policy_set.end())
        it = policy_set.begin();

    cur_offset = it->get_key();

    // try another addr if previous addr failed
    if (params.try_another_addr) {
        if (it.get_pointer() == prev && policy_set.size() > 1) {
            ++it;

            if (it == policy_set.end())
                it = policy_set.begin();
        }
    }

    AddrInfo *addr = it.get_pointer();
    policy_set.erase(addr);
    addr->step_forward();
    policy_set.insert(addr);

    addr->inc_ref();
    return addr;
}

} // namespace coke
