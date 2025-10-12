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

#ifndef COKE_NSPOLICY_WEIGHTED_LEAST_CONN_POLICY_H
#define COKE_NSPOLICY_WEIGHTED_LEAST_CONN_POLICY_H

#include "coke/nspolicy/weighted_policy_base.h"

namespace coke {

class WeightedLeastConnPolicy;
class WeightedLeastConnAddressInfo;

template<>
struct NSPolicyTrait<WeightedLeastConnPolicy> {
    using Policy = WeightedLeastConnPolicy;
    using AddrInfo = WeightedLeastConnAddressInfo;

    using PolicyMutex = std::mutex;
    using PolicyModifyLock = std::unique_lock<PolicyMutex>;
    using PolicySelectLock = std::unique_lock<PolicyMutex>;
};

class WeightedLeastConnAddressInfo : public AddressInfo {
protected:
    struct AddrCmp {
        using AddrInfo = WeightedLeastConnAddressInfo;
        bool operator()(const AddrInfo &left, const AddrInfo &right) const
        {
            return left.get_key() < right.get_key();
        }
    };

    static constexpr uint64_t SEATINGS = (1ULL << 16);

public:
    WeightedLeastConnAddressInfo(const std::string &host,
                                 const std::string &port,
                                 const AddressParams &params)
        : AddressInfo(host, port, params), conn_count(0), virtual_count(0),
          key(0)
    {
    }

    uint64_t get_conn_count() const { return conn_count; }
    uint64_t get_key() const { return key; }

protected:
    void inc_conn_count()
    {
        ++conn_count;
        update_key();
    }

    void dec_conn_count()
    {
        --conn_count;

        if (virtual_count) {
            --conn_count;
            --virtual_count;
        }

        update_key();
    }

    void set_conn_by_key(uint64_t k)
    {
        uint64_t new_count = k * get_weight() / SEATINGS;

        if (new_count > conn_count) {
            virtual_count += new_count - conn_count;
            conn_count = new_count;
        }
        else if (new_count < conn_count) {
            uint64_t diff = conn_count - new_count;
            if (diff < virtual_count)
                diff = virtual_count;

            conn_count -= diff;
            virtual_count -= diff;
        }

        update_key();
    }

    void update_key()
    {
        if (conn_count != virtual_count)
            key = SEATINGS * conn_count / get_weight();
        else
            key = 0;
    }

protected:
    uint64_t conn_count;
    uint64_t virtual_count;
    uint64_t key;

    RBTreeNode policy_node;

    friend WeightedPolicyBase<WeightedLeastConnPolicy>;
    friend WeightedLeastConnPolicy;
};

class WeightedLeastConnPolicy
    : public WeightedPolicyBase<WeightedLeastConnPolicy> {
protected:
    using PolicyBase = WeightedPolicyBase<WeightedLeastConnPolicy>;
    using PolicySet =
        RBTree<AddrInfo, &AddrInfo::policy_node, AddrInfo::AddrCmp>;

    static constexpr uint64_t SEATINGS = AddrInfo::SEATINGS;

public:
    WeightedLeastConnPolicy() : WeightedLeastConnPolicy(NSPolicyParams{}) {}

    WeightedLeastConnPolicy(const NSPolicyParams &params) : PolicyBase(params)
    {
    }

    virtual ~WeightedLeastConnPolicy() { policy_set.clear(); }

protected:
    void add_to_policy(AddrInfo *addr);
    void remove_from_policy(AddrInfo *addr);

    std::size_t policy_set_size() const { return policy_set.size(); }

    AddrInfo *select_addr(const ParsedURI &uri, const SelectHistory &history);
    void handle_finish(AddrInfo *addr);

protected:
    PolicySet policy_set;

    friend PolicyBase;
};

} // namespace coke

#endif // COKE_NSPOLICY_WEIGHTED_LEAST_CONN_POLICY_H
