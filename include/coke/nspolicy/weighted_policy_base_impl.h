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

#ifndef COKE_NSPOLICY_WEIGHTED_POLICY_BASE_IMPL_H
#define COKE_NSPOLICY_WEIGHTED_POLICY_BASE_IMPL_H

#include "coke/nspolicy/weighted_policy_base.h"

namespace coke {

template<typename PolicyImpl>
bool WeightedPolicyBase<PolicyImpl>::add_address(const std::string &host,
                                                 const std::string &port,
                                                 const AddressParams &params,
                                                 bool replace)
{
    AddrSetLock addr_set_lk(addr_set_mtx);

    auto old_it      = addr_set.find(HostPortRef{host, port});
    bool has_address = (old_it != addr_set.end());

    if (has_address && !replace)
        return false;

    AddrUniquePtr addr_ptr(new AddrInfo(host, port, params));
    PolicyModifyLock lk(policy_mtx);
    AddrInfo *addr = addr_ptr.get();

    if (has_address) {
        AddrInfo *old_addr = addr_cast(old_it.get_pointer());

        if (old_addr->get_state() == ADDR_STATE_DISABLED)
            remove_from_recover_list(old_addr);
        else
            self().remove_from_policy(old_addr);

        total_weight -= old_addr->get_weight();
        addr_set.erase(old_addr);
        old_addr->set_state(ADDR_STATE_REMOVED);
        old_addr->dec_ref();
    }

    total_weight += addr->get_weight();
    self().add_to_policy(addr);
    addr_set.insert(addr);
    addr_ptr.release();

    return true;
}

template<typename PolicyImpl>
bool WeightedPolicyBase<PolicyImpl>::break_address(const std::string &host,
                                                   const std::string &port)
{
    AddrSetLock addr_set_lk(addr_set_mtx);

    auto it = addr_set.find(HostPortRef{host, port});
    if (it == addr_set.end())
        return false;

    PolicyModifyLock lk(policy_mtx);
    AddrInfo *addr = addr_cast(it.get_pointer());

    if (addr->get_state() == ADDR_STATE_DISABLED)
        remove_from_recover_list(addr);
    else
        self().remove_from_policy(addr);

    auto steady_ms = steady_milliseconds();

    addr->set_state(ADDR_STATE_DISABLED);
    addr->fail_marks      = params.max_fail_marks;
    addr->first_fail_time = steady_ms;
    addr->recover_at_time = steady_ms + params.break_timeout_ms;

    add_to_recover_list(addr);

    return true;
}

template<typename PolicyImpl>
bool WeightedPolicyBase<PolicyImpl>::recover_address(const std::string &host,
                                                     const std::string &port)
{
    AddrSetLock addr_set_lk(addr_set_mtx);

    auto it = addr_set.find(HostPortRef{host, port});
    if (it == addr_set.end())
        return false;

    PolicyModifyLock lk(policy_mtx);
    AddrInfo *addr = addr_cast(it.get_pointer());

    if (addr->get_state() == ADDR_STATE_DISABLED) {
        remove_from_recover_list(addr);
        self().add_to_policy(addr);
    }

    addr->set_state(ADDR_STATE_GOOD);
    addr->fail_marks      = 0;
    addr->first_fail_time = 0;
    addr->recover_at_time = 0;

    return true;
}

template<typename PolicyImpl>
bool WeightedPolicyBase<PolicyImpl>::remove_address(const std::string &host,
                                                    const std::string &port)
{
    AddrSetLock addr_set_lk(addr_set_mtx);

    auto it = addr_set.find(HostPortRef{host, port});
    if (it == addr_set.end())
        return false;

    PolicyModifyLock lk(policy_mtx);
    AddrInfo *addr = addr_cast(it.get_pointer());

    if (addr->get_state() == ADDR_STATE_DISABLED)
        remove_from_recover_list(addr);
    else
        self().remove_from_policy(addr);

    total_weight -= addr->get_weight();
    addr_set.erase(it);
    addr->set_state(ADDR_STATE_REMOVED);
    addr->dec_ref();

    return true;
}

template<typename PolicyImpl>
AddressInfo *
WeightedPolicyBase<PolicyImpl>::select_address(const ParsedURI &uri,
                                               const SelectHistory &history)
{
    if constexpr (efficient_select) {
        PolicySelectLock lk(policy_mtx);
        if (!need_recover())
            return self().select_addr(uri, history);
    }

    PolicyModifyLock lk(policy_mtx);
    if (need_recover()) {
        try_recover(available_weight == 0);
    }

    return self().select_addr(uri, history);
}

template<typename PolicyImpl>
void WeightedPolicyBase<PolicyImpl>::addr_success(AddressInfo *addr)
{
    if constexpr (fast_success) {
        if (addr->get_state() != ADDR_STATE_GOOD) {
            if (params.enable_auto_break_recover) {
                PolicyModifyLock lk(policy_mtx);
                handle_success(addr_cast(addr));
            }
            else {
                addr_finish(addr);
            }
        }
    }
    else {
        if (params.enable_auto_break_recover) {
            PolicyModifyLock lk(policy_mtx);
            handle_success(addr_cast(addr));
        }
        else {
            addr_finish(addr);
        }
    }
}

template<typename PolicyImpl>
void WeightedPolicyBase<PolicyImpl>::addr_failed(AddressInfo *addr)
{
    if (params.enable_auto_break_recover) {
        PolicyModifyLock lk(policy_mtx);
        handle_failed(addr_cast(addr));
    }
    else {
        addr_finish(addr);
    }
}

template<typename PolicyImpl>
void WeightedPolicyBase<PolicyImpl>::addr_finish(AddressInfo *addr)
{
    if constexpr (!no_need_finish) {
        PolicyModifyLock lk(policy_mtx);
        self().handle_finish(addr_cast(addr));
    }
}

template<typename PolicyImpl>
void WeightedPolicyBase<PolicyImpl>::handle_success(AddrInfo *addr)
{
    self().handle_finish(addr);

    int addr_state = addr->get_state();
    if (addr_state == ADDR_STATE_REMOVED || addr_state == ADDR_STATE_GOOD)
        return;

    if (addr_state == ADDR_STATE_DISABLED)
        return;

    if (addr->fail_marks > params.success_dec_marks)
        addr->fail_marks -= params.success_dec_marks;
    else
        addr->fail_marks = 0;

    if (addr->fail_marks == 0) {
        addr->set_state(ADDR_STATE_GOOD);
        addr->first_fail_time = 0;
    }
    else {
        // The address is still in failing state, reset first fail time.
        addr->first_fail_time = steady_milliseconds();
    }
}

template<typename PolicyImpl>
void WeightedPolicyBase<PolicyImpl>::handle_failed(AddrInfo *addr)
{
    self().handle_finish(addr);

    int addr_state = addr->get_state();
    if (addr_state == ADDR_STATE_REMOVED || addr_state == ADDR_STATE_DISABLED)
        return;

    int64_t steady_ms = steady_milliseconds();
    bool was_good     = (addr_state == ADDR_STATE_GOOD);
    bool disable;

    addr->fail_marks += params.fail_inc_marks;
    if (addr->fail_marks > params.max_fail_marks)
        addr->fail_marks = params.max_fail_marks;

    disable = addr->fail_marks >= params.max_fail_marks ||
              (addr->first_fail_time &&
               steady_ms - addr->first_fail_time > params.max_fail_ms);

    if (disable) {
        addr->set_state(ADDR_STATE_DISABLED);
        addr->recover_at_time = steady_ms + params.break_timeout_ms;
        self().remove_from_policy(addr);
        add_to_recover_list(addr);
    }
    else if (was_good) {
        addr->set_state(ADDR_STATE_FAILING);
        addr->first_fail_time = steady_ms;
    }
}

template<typename PolicyImpl>
std::vector<bool> WeightedPolicyBase<PolicyImpl>::add_addresses(
    const std::vector<AddressPack> &addr_packs, bool replace)
{
    std::vector<bool> rets;
    std::vector<AddrUniquePtr> addrs;
    std::vector<AddrInfo *> removed;

    rets.reserve(addr_packs.size());
    addrs.reserve(addr_packs.size());

    AddrSetLock addr_set_lk(addr_set_mtx);

    for (const auto &pack : addr_packs) {
        auto it          = addr_set.find(HostPortRef{pack.host, pack.port});
        bool has_address = (it != addr_set.end());

        if (has_address && !replace) {
            rets.push_back(false);
            continue;
        }

        if (has_address)
            removed.push_back(addr_cast(it.get_pointer()));

        AddrInfo *addr = new AddrInfo(pack.host, pack.port, pack.params);
        addrs.emplace_back(addr);
        rets.push_back(true);
    }

    PolicyModifyLock lk(policy_mtx);

    for (AddrInfo *addr : removed) {
        if (addr->get_state() == ADDR_STATE_DISABLED)
            remove_from_recover_list(addr);
        else
            self().remove_from_policy(addr);

        total_weight -= addr->get_weight();
        addr_set.erase(addr);
        addr->set_state(ADDR_STATE_REMOVED);
        addr->dec_ref();
    }

    for (AddrUniquePtr &addr_ptr : addrs) {
        AddrInfo *addr = addr_ptr.get();

        total_weight += addr->get_weight();
        self().add_to_policy(addr);
        addr_set.insert(addr);
        addr_ptr.release();
    }

    return rets;
}

template<typename PolicyImpl>
std::vector<bool> WeightedPolicyBase<PolicyImpl>::remove_addresses(
    const std::vector<HostPortPack> &addr_packs)
{
    std::vector<bool> rets;
    std::vector<AddrInfo *> removed;

    rets.reserve(addr_packs.size());
    removed.reserve(addr_packs.size());

    AddrSetLock addr_set_lk(addr_set_mtx);

    for (const auto &pack : addr_packs) {
        auto it = addr_set.find(HostPortRef{pack.host, pack.port});
        if (it != addr_set.end()) {
            removed.push_back(addr_cast(it.get_pointer()));
            rets.push_back(true);
        }
        else {
            rets.push_back(false);
        }
    }

    PolicyModifyLock lk(policy_mtx);

    for (AddrInfo *addr : removed) {
        if (addr->get_state() == ADDR_STATE_DISABLED)
            remove_from_recover_list(addr);
        else
            self().remove_from_policy(addr);

        total_weight -= addr->get_weight();
        addr_set.erase(addr);
        addr->set_state(ADDR_STATE_REMOVED);
        addr->dec_ref();
    }

    return rets;
}

} // namespace coke

#endif // COKE_NSPOLICY_WEIGHTED_POLICY_BASE_IMPL_H
