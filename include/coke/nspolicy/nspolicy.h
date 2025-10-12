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

#ifndef COKE_NSPOLICY_NSPOLICY_H
#define COKE_NSPOLICY_NSPOLICY_H

#include <chrono>
#include <mutex>

#include "coke/nspolicy/address_info.h"
#include "workflow/WFNameService.h"

namespace coke {

struct NSPolicyParams {
    /**
     * If this option is false, the policy will not break or recover addresses,
     * user can use NSPolicy::break_address and NSPolicy::recover_address to
     * control the address state.
     */
    bool enable_auto_break_recover{true};

    /**
     * If all addresses are broken, when the first address reaches break
     * timeout, try to recover all addresses.
     */
    bool fast_recover{true};

    /**
     * If the request is retried after a failure, try to choose a different
     * address for it than the previous one.
     */
    bool try_another_addr{true};

    /**
     * When the available weight ratio is less than this percentage, some
     * requests will fail directly when obtaining addresses to prevent a small
     * number of addresses from being overloaded.
     * The value of this parameter should be in [0, 100] and defaults to 0.
     */
    uint32_t min_available_percent{0};

    /**
     * When the fail marks reaches max_fail_marks, the address will be set to
     * disabled state.
     * The value of this parameter must greater than 0.
     */
    uint32_t max_fail_marks{100};

    /**
     * When an address fails continuously for max_fail_ms time, it will be set
     * to disabled state.
     * When qps is low, this option will take effect faster than max_fail_marks.
     */
    uint32_t max_fail_ms{10 * 1000};

    /**
     * When a request to the address succeeds, the fail marks will be
     * decremented by success_dec_marks. This value must in (0, max_fail_marks].
     */
    uint32_t success_dec_marks{1};

    /**
     * When a request to the address fails, the fail marks will be incremented
     * by fail_inc_marks. This value must in (0, max_fail_marks].
     */
    uint32_t fail_inc_marks{1};

    /**
     * When an address is broken, try to recover it after break_timeout_ms.
     * The time of recover may be advanced by other options.
     */
    uint32_t break_timeout_ms{60 * 1000};
};

class NSPolicy : public WFNSPolicy {
protected:
    static constexpr int64_t INF_RECOVER_TIME = INT64_MAX;

    using RecoverList = List<AddressInfo, &AddressInfo::recover_node>;
    using AddrSet =
        RBTree<AddressInfo, &AddressInfo::addr_node, AddressInfoCmp>;
    using AddrSetMutex = std::mutex;
    using AddrSetLock = std::unique_lock<AddrSetMutex>;

    static inline int64_t steady_milliseconds()
    {
        using std::chrono::milliseconds;
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<milliseconds>(now);
        return static_cast<int64_t>(ms.count());
    }

public:
    using SelectHistory = std::vector<AddressInfo *>;

public:
    NSPolicy(const NSPolicyParams &ns_params)
        : params(ns_params), next_recover_time(INF_RECOVER_TIME)
    {
        if (params.min_available_percent > 100)
            params.min_available_percent = 100;

        if (params.max_fail_marks == 0)
            params.max_fail_marks = 1;

        if (params.max_fail_ms == 0)
            params.max_fail_ms = 1;

        if (params.success_dec_marks == 0)
            params.success_dec_marks = 1;

        if (params.fail_inc_marks == 0)
            params.fail_inc_marks = 1;
    }

    NSPolicy(const NSPolicy &) = delete;

    virtual ~NSPolicy();

    virtual WFRouterTask *
    create_router_task(const WFNSParams *params,
                       router_callback_t callback) override;

    virtual void success(RouteManager::RouteResult *result,
                         WFNSTracing *tracing,
                         CommTarget *target) override final;

    virtual void failed(RouteManager::RouteResult *result, WFNSTracing *tracing,
                        CommTarget *target) override final;

    /**
     * @brief Returns the number of addresses in the policy.
     */
    std::size_t address_count() const
    {
        AddrSetLock addr_set_lk(addr_set_mtx);
        return addr_set.size();
    }

    /**
     * @brief Returns the number of available addresses.
     */
    virtual std::size_t available_address_count() const = 0;

    /**
     * @brief Checks whether the policy contains the specified host and port.
     *
     * @param host The host of address.
     * @param port The port of address.
     * @return true if contains, false otherwise.
     */
    bool has_address(const std::string &host, const std::string &port) const
    {
        AddrSetLock addr_set_lk(addr_set_mtx);
        return addr_set.contains(HostPortRef{host, port});
    }

    /**
     * @brief Get the address information for the specified host and port.
     * @param host The host of address.
     * @param port The port of address.
     * @note The returned pointer must be checked for validity before use.
     *       If the address is not found, the return value is nullptr.
     *       If the return value is not nullptr, the caller must call the
     *       addr->dec_ref() after use.
     */
    [[nodiscard]]
    AddressInfo *get_address(const std::string &host,
                             const std::string &port) const;

    /**
     * @brief Get all addresses in the policy.
     * @return std::vector<AddressPack> A vector of AddressPack objects.
     */
    std::vector<AddressPack> get_all_address() const;

    /**
     * @brief Adds an address to the policy.
     * @param host The host of address.
     * @param port The port of address.
     * @param params Additional parameters for the address.
     * @param replace If true, replaces an existing address with the same host
     *                and port.
     * @return true if the address was added successfully, false otherwise.
     */
    virtual bool add_address(const std::string &host, const std::string &port,
                             const AddressParams &params,
                             bool replace = false) = 0;

    /**
     * @brief Breaks down the address with the given host and port.
     * @param host The host of address.
     * @param port The port of address.
     * @return true if the address is broken down, false if not exists.
     */
    virtual bool break_address(const std::string &host,
                               const std::string &port) = 0;

    /**
     * @brief Recovers the address with the given host and port.
     * @param host The host of address.
     * @param port The port of address.
     * @return true if the address is recoverd, false if not exists.
     */
    virtual bool recover_address(const std::string &host,
                                 const std::string &port) = 0;

    /**
     * @brief Removes the address with the given host and port.
     * @param host The host of address.
     * @param port The port of address.
     * @return true if the address was removed, false if not exists.
     */
    virtual bool remove_address(const std::string &host,
                                const std::string &port) = 0;

    /**
     * @brief Selects an address based on the given URI and history.
     * @param uri Some policy may use the URI to select an address.
     * @param history A group of pointers to AddressInfo objects representing
     *                the history of previously used addresses.
     * @return A pointer to the selected AddressInfo object, if no address is
     *         selected, return nullptr.
     * @note   If the return value is not nullptr, the caller must call the
     *         addr->inc_ref() after use.
     * @note   Every not nullptr return value must be notified by addr_success,
     *         addr_failed or addr_finish exactly once.
     */
    [[nodiscard]]
    virtual AddressInfo *select_address(const ParsedURI &uri,
                                        const SelectHistory &history) = 0;

    /**
     * @brief Notify the policy that the address is successfully used.
     * @param addr AddressInfo * acquired by previous select_address call.
     */
    virtual void addr_success(AddressInfo *addr) = 0;

    /**
     * @brief Notify the policy that the address is failed to use.
     * @param addr AddressInfo * acquired by previous select_address call.
     */
    virtual void addr_failed(AddressInfo *addr) = 0;

    /**
     * @brief Notify the policy that the address used, but do not perform
     *        breaking or recovering operations.
     * @param addr AddressInfo * acquired by previous select_address call.
     */
    virtual void addr_finish(AddressInfo *addr) = 0;

    /**
     * @brief Adds a vector of addresses to the policy.
     *
     * The default implementation is to call add_address for each address in
     * the vector. Derived classes can override this method to provide a more
     * efficient implementation.
     *
     * @param addrs A vector of AddressPack representing the addresses.
     * @param replace Whether to replace the existing addresses.
     * @return Whether the addresses are added successfully.
     */
    virtual std::vector<bool>
    add_addresses(const std::vector<AddressPack> &addrs, bool replace = false);

    /**
     * @brief Removes the specified addresses in the vector.
     *
     * The default implementation is to call remove_address for each address in
     * the vector. Derived classes can override this method to provide a more
     * efficient implementation.
     *
     * @param addrs A vector of HostPortPack representing the addresses.
     * @return Whether the addresses are removed successfully.
     *
     */
    virtual std::vector<bool>
    remove_addresses(const std::vector<HostPortPack> &addrs);

    /**
     * @brief Removes the specified addresses in the vector.
     *
     * This function convert AddressPack to HostPortPack and call the previous
     * remove_addresses function.
     *
     * @param addrs A vector of AddressPack representing the addresses.
     * @return Whether the addresses are removed successfully.
     */
    std::vector<bool> remove_addresses(const std::vector<AddressPack> &addrs);

protected:
    void add_to_recover_list(AddressInfo *addr);

    void remove_from_recover_list(AddressInfo *addr);

    bool need_recover() const
    {
        if (!params.enable_auto_break_recover)
            return false;

        if (next_recover_time == INF_RECOVER_TIME) [[likely]]
            return false;

        int64_t steady_ms = steady_milliseconds();
        if (steady_ms >= next_recover_time)
            return true;

        return false;
    }

    /**
     * @brief Try to recover addresses in recover_list.
     *
     * Policy implmentation will call try_recover, and try_recover will call
     * recover_addr to recover addresses.
     */
    void try_recover(bool all_break);

    virtual void recover_addr(AddressInfo *addr) = 0;

protected:
    NSPolicyParams params;

    mutable AddrSetMutex addr_set_mtx;
    AddrSet addr_set;

    // protected by policy mutex

    RecoverList recover_list;
    int64_t next_recover_time;

    friend class BasicRouterTask;
};

} // namespace coke

#endif // COKE_NSPOLICY_NSPOLICY_H
