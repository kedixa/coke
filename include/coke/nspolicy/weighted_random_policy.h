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

#ifndef COKE_NSPOLICY_WEIGHTED_RANDOM_POLICY_H
#define COKE_NSPOLICY_WEIGHTED_RANDOM_POLICY_H

#include <shared_mutex>
#include <utility>

#include "coke/detail/binary_indexed_tree.h"
#include "coke/nspolicy/weighted_policy_base.h"

namespace coke {

class WeightedRandomPolicy;
class WeightedRandomAddressInfo;

template<>
struct NSPolicyTrait<WeightedRandomPolicy> {
    using Policy = WeightedRandomPolicy;
    using AddrInfo = WeightedRandomAddressInfo;

    using PolicyMutex = std::shared_mutex;
    using PolicyModifyLock = std::unique_lock<PolicyMutex>;
    using PolicySelectLock = std::shared_lock<PolicyMutex>;

    using EfficientSelectTag = void;
    using FastSuccessTag = void;
    using NoNeedFinishTag = void;
};

class WeightedRandomAddressInfo : public AddressInfo {
public:
    static constexpr std::size_t POS_NULL = 0;

    WeightedRandomAddressInfo(const std::string &host, const std::string &port,
                              const AddressParams &params)
        : AddressInfo(host, port, params), addr_pos(POS_NULL)
    {
    }

    ~WeightedRandomAddressInfo() = default;

protected:
    std::size_t addr_pos;

    friend WeightedPolicyBase<WeightedRandomPolicy>;
    friend WeightedRandomPolicy;
};

class WeightedRandomPolicy : public WeightedPolicyBase<WeightedRandomPolicy> {
public:
    using PolicyBase = WeightedPolicyBase<WeightedRandomPolicy>;
    static constexpr std::size_t POS_NULL = AddrInfo::POS_NULL;

    WeightedRandomPolicy() : WeightedRandomPolicy(NSPolicyParams{}) {}

    WeightedRandomPolicy(const NSPolicyParams &params) : PolicyBase(params)
    {
        // because BinaryIndexedTree start at pos 1
        policy_addrs.push_back(nullptr);
    }

    virtual ~WeightedRandomPolicy() = default;

protected:
    void add_to_policy(AddrInfo *addr);
    void remove_from_policy(AddrInfo *addr);

    AddrInfo *select_addr(const ParsedURI &uri, const SelectHistory &history);

    std::size_t policy_set_size() const { return tree.size(); }

    void handle_finish(AddrInfo *) {}

protected:
    detail::BinaryIndexedTree<uint64_t> tree;
    std::vector<AddrInfo *> policy_addrs;

    friend PolicyBase;
};

} // namespace coke

#endif // COKE_NSPOLICY_WEIGHTED_RANDOM_POLICY_H
