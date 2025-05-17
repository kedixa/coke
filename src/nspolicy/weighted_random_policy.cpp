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

#include "coke/nspolicy/weighted_random_policy.h"

#include "coke/detail/random.h"
#include "coke/nspolicy/weighted_policy_base_impl.h"

namespace coke {

template class WeightedPolicyBase<WeightedRandomPolicy>;

using AddrInfo = WeightedRandomAddressInfo;

void WeightedRandomPolicy::add_to_policy(AddrInfo *addr)
{
    auto pos       = tree.add_element(addr->get_weight());
    addr->addr_pos = pos;

    policy_addrs.push_back(addr);
    available_weight += addr->get_weight();
}

void WeightedRandomPolicy::remove_from_policy(AddrInfo *addr)
{
    if (addr->addr_pos != tree.size()) {
        AddrInfo *last       = policy_addrs.back();
        uint16_t addr_weight = addr->get_weight();
        uint16_t last_weight = last->get_weight();

        tree.decrease(last->addr_pos, last_weight);

        if (addr_weight > last_weight)
            tree.decrease(addr->addr_pos, addr_weight - last_weight);
        else
            tree.increase(addr->addr_pos, last_weight - addr_weight);

        policy_addrs[addr->addr_pos] = last;
        last->addr_pos               = addr->addr_pos;
    }

    tree.remove_last_element();
    policy_addrs.pop_back();
    tree.shrink();

    addr->addr_pos = POS_NULL;
    available_weight -= addr->get_weight();
}

AddrInfo *WeightedRandomPolicy::select_addr(const ParsedURI &uri,
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

    uint64_t prev_weight;
    bool flag = false;

    // try another addr if previous addr failed
    if (params.try_another_addr && prev && in_policy(prev)) {
        // whether prev is the only addr
        prev_weight = prev->get_weight();
        flag        = prev_weight < available_weight;
    }

    uint64_t rnd = rand_u64();

    if (!flag) {
        rnd = rnd % available_weight;
    }
    else {
        uint64_t tmp_available = available_weight - prev_weight;
        uint64_t prev_sum      = tree.prefix_sum(prev->addr_pos - 1);

        rnd = rnd % tmp_available;
        if (rnd >= prev_sum)
            rnd += prev_weight;
    }

    std::size_t pos = tree.find_pos(rnd);
    AddrInfo *addr  = policy_addrs[pos];
    addr->inc_ref();

    return addr;
}

} // namespace coke
