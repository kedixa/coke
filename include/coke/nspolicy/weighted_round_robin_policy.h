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

#ifndef COKE_NSPOLICY_WEIGHTED_ROUND_ROBIN_POLICY_H
#define COKE_NSPOLICY_WEIGHTED_ROUND_ROBIN_POLICY_H

#include "coke/nspolicy/weighted_policy_base.h"

namespace coke {

class WeightedRoundRobinPolicy;
class WeightedRoundRobinAddressInfo;

template<>
struct NSPolicyTrait<WeightedRoundRobinPolicy> {
    using Policy   = WeightedRoundRobinPolicy;
    using AddrInfo = WeightedRoundRobinAddressInfo;

    using PolicyMutex      = std::mutex;
    using PolicyModifyLock = std::unique_lock<PolicyMutex>;
    using PolicySelectLock = std::unique_lock<PolicyMutex>;

    using FastSuccessTag = void;
};

class WeightedRoundRobinAddressInfo : public AddressInfo {
protected:
    struct AddrCmp {
        using AddrInfo = WeightedRoundRobinAddressInfo;

        bool operator()(const AddrInfo &left, const AddrInfo &right) const
        {
            return left.get_key() < right.get_key();
        }

        bool operator()(const AddrInfo &left, const uint64_t &key) const
        {
            return left.get_key() < key;
        }

        bool operator()(const uint64_t &key, const AddrInfo &right) const
        {
            return key < right.get_key();
        }
    };

    static constexpr uint64_t SEATINGS         = (1ULL << 20);
    static constexpr uint64_t VIRTUAL_SEATINGS = (SEATINGS << 10);

    // The key of address info wraps at VIRTUAL_SEATINGS, it should be much
    // greater than SEATINGS to prevent key overflow after a full round.

    static_assert(VIRTUAL_SEATINGS > SEATINGS);
    static_assert(SEATINGS > ADDRESS_WEIGHT_MAX);

public:
    WeightedRoundRobinAddressInfo(const std::string &host,
                                  const std::string &port,
                                  const AddressParams &params)
        : AddressInfo(host, port, params), step(0), offset(0), key(0)
    {
    }

    uint64_t get_key() const { return key; }

protected:
    void reset_offset(uint64_t key_offset)
    {
        offset = key_offset;
        step   = 0;
        update_key();
    }

    void step_forward()
    {
        ++step;
        update_key();
    }

    void update_key()
    {
        key = SEATINGS * step / get_weight() + offset;

        if (key >= VIRTUAL_SEATINGS) [[unlikely]] {
            key    = key % VIRTUAL_SEATINGS;
            offset = key;
            step   = 0;
        }
    }

protected:
    uint64_t step;
    uint64_t offset;
    uint64_t key;

    detail::RBTreeNode policy_node;

    friend WeightedPolicyBase<WeightedRoundRobinPolicy>;
    friend WeightedRoundRobinPolicy;
};

class WeightedRoundRobinPolicy
    : public WeightedPolicyBase<WeightedRoundRobinPolicy> {
protected:
    using PolicyBase = WeightedPolicyBase<WeightedRoundRobinPolicy>;
    using PolicySet =
        detail::RBTree<AddrInfo, &AddrInfo::policy_node, AddrInfo::AddrCmp>;

    static constexpr uint64_t SEATINGS = AddrInfo::SEATINGS;

public:
    WeightedRoundRobinPolicy() : WeightedRoundRobinPolicy(NSPolicyParams{}) {}

    WeightedRoundRobinPolicy(const NSPolicyParams &params) : PolicyBase(params)
    {
    }

    virtual ~WeightedRoundRobinPolicy() { policy_set.clear_unsafe(); }

protected:
    void add_to_policy(AddrInfo *addr);
    void remove_from_policy(AddrInfo *addr);

    std::size_t policy_set_size() const { return policy_set.size(); }

    AddrInfo *select_addr(const ParsedURI &uri, const SelectHistory &history);
    void handle_finish(AddrInfo *) {}

protected:
    PolicySet policy_set;
    uint64_t cur_offset{0};

    friend PolicyBase;
};

} // namespace coke

#endif // COKE_NSPOLICY_WEIGHTED_ROUND_ROBIN_POLICY_H
