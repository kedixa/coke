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

#ifndef COKE_NSPOLICY_WEIGHTED_POLICY_BASE_H
#define COKE_NSPOLICY_WEIGHTED_POLICY_BASE_H

#include <memory>

#include "coke/nspolicy/nspolicy.h"

namespace coke {

template<typename Policy>
struct NSPolicyTrait;

template<typename PolicyImpl>
class WeightedPolicyBase : public NSPolicy {
protected:
    using Policy      = PolicyImpl;
    using PolicyTrait = NSPolicyTrait<Policy>;

    using AddrInfo         = typename PolicyTrait::AddrInfo;
    using PolicyMutex      = typename PolicyTrait::PolicyMutex;
    using PolicyModifyLock = typename PolicyTrait::PolicyModifyLock;
    using PolicySelectLock = typename PolicyTrait::PolicySelectLock;
    using AddrUniquePtr    = std::unique_ptr<AddrInfo, AddressInfoDeleter>;

    static constexpr bool efficient_select = requires {
        typename PolicyTrait::EfficientSelectTag;
    };

    static constexpr bool fast_success = requires {
        typename PolicyTrait::FastSuccessTag;
    };

    static constexpr bool need_finish = requires {
        typename PolicyTrait::NeedFinishTag;
    };

    static AddrInfo *addr_cast(AddressInfo *addr)
    {
        return static_cast<AddrInfo *>(addr);
    }

public:
    WeightedPolicyBase(const NSPolicyParams &params)
        : NSPolicy(params), total_weight(0), available_weight(0)
    {
    }

    virtual ~WeightedPolicyBase() = default;

    virtual std::size_t available_address_count() const
    {
        PolicyModifyLock lk(policy_mtx);
        return self().policy_set_size();
    }

    virtual bool add_address(const std::string &host, const std::string &port,
                             const AddressParams &params,
                             bool replace = false) override;

    virtual bool break_address(const std::string &host,
                               const std::string &port) override;

    virtual bool recover_address(const std::string &host,
                                 const std::string &port) override;

    virtual bool remove_address(const std::string &host,
                                const std::string &port) override;

    virtual AddressInfo *select_address(const ParsedURI &uri,
                                        const SelectHistory &history) override;

    virtual void addr_success(AddressInfo *addr) override;

    virtual void addr_failed(AddressInfo *addr) override;

    virtual void addr_finish(AddressInfo *addr) override;

    virtual std::vector<bool>
    add_addresses(const std::vector<AddressPack> &addrs,
                  bool replace = false) override;

    virtual std::vector<bool>
    remove_addresses(const std::vector<HostPortPack> &addrs) override;

protected:
    void handle_success(AddrInfo *addr);
    void handle_failed(AddrInfo *addr);

    bool in_policy(AddrInfo *addr)
    {
        int state = addr->get_state();
        return (state == ADDR_STATE_GOOD || state == ADDR_STATE_FAILING);
    }

    virtual void recover_addr(AddressInfo *addr) override
    {
        // Already locked before calling try_recover.
        self().add_to_policy(addr_cast(addr));
    }

private:
    Policy &self() & { return static_cast<Policy &>(*this); }

    const Policy &self() const & { return static_cast<const Policy &>(*this); }

protected:
    mutable PolicyMutex policy_mtx;
    uint64_t total_weight;
    uint64_t available_weight;
};

} // namespace coke

#endif // COKE_NSPOLICY_WEIGHTED_POLICY_BASE_H
