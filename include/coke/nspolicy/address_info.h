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

#ifndef COKE_NSPOLICY_ADDRESS_INFO_H
#define COKE_NSPOLICY_ADDRESS_INFO_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "coke/detail/list.h"
#include "coke/detail/rbtree.h"
#include "coke/detail/ref_counted.h"
#include "coke/global.h"

namespace coke {

enum {
    ADDR_STATE_GOOD     = 0,
    ADDR_STATE_FAILING  = 1,
    ADDR_STATE_DISABLED = 2,
    ADDR_STATE_REMOVED  = 3,
};

constexpr static uint16_t ADDRESS_WEIGHT_MAX = 1000;


struct AddressParams {
    EndpointParams endpoint_params = ENDPOINT_PARAMS_DEFAULT;
    unsigned dns_ttl_default{3600};
    unsigned dns_ttl_min{60};

    uint16_t weight{100}; // must in [1, ADDRESS_WEIGHT_MAX]
};


struct AddressPack {
    int state{0};
    std::string host;
    std::string port;
    AddressParams params;
};


struct HostPortPack {
    std::string host;
    std::string port;
};


class AddressInfo : public detail::RefCounted<AddressInfo> {
public:
    AddressInfo(const std::string &host, const std::string &port,
                const AddressParams &params)
        : state(ADDR_STATE_GOOD),
          host(host), port(port), addr_params(params),
          fail_cnt(0), first_fail_time(0), recover_at_time(0)
    {
        if (addr_params.weight == 0)
            addr_params.weight = 1;
        else if (addr_params.weight > ADDRESS_WEIGHT_MAX)
            addr_params.weight = ADDRESS_WEIGHT_MAX;
    }

    /**
     * @brief AddressInfo is not copyable.
     */
    AddressInfo(const AddressInfo &) = delete;

    const std::string &get_host() const & { return host; }
    const std::string &get_port() const & { return port; }
    const AddressParams &get_addr_params() const & { return addr_params; }

    uint16_t get_weight() const {
        return addr_params.weight;
    }

    /**
     * @brief Get the state of the address, one of ADDR_STATE_GOOD,
     *        ADDR_STATE_FAILING, ADDR_STATE_DISABLED, ADDR_STATE_REMOVED.
     */
    int get_state() const {
        return state.load(std::memory_order_relaxed);
    }

protected:
    void set_state(int s) {
        state.store(s, std::memory_order_relaxed);
    }

    virtual ~AddressInfo() = default;

protected:
    std::atomic<uint32_t> state;

    std::string host;
    std::string port;
    AddressParams addr_params;

    // Protected by policy's lock

    uint64_t fail_cnt;
    int64_t first_fail_time;
    int64_t recover_at_time;

    detail::ListNode recover_node;
    detail::RBTreeNode addr_node;

    friend class NSPolicy;
    friend std::default_delete<AddressInfo>;
};


struct AddressInfoDeleter {
    void operator()(AddressInfo *addr) const {
        addr->dec_ref();
    }
};


class HostPortRef {
public:
    HostPortRef(const std::string &host, const std::string &port)
        : host(host), port(port)
    { }

    HostPortRef(const AddressInfo &addr)
        : host(addr.get_host()), port(addr.get_port())
    { }

    HostPortRef(const HostPortRef &other) = default;
    ~HostPortRef() = default;

    bool operator< (const HostPortRef &other) const noexcept {
        int ret = host.compare(other.host);
        if (ret == 0)
            return port < other.port;
        return ret < 0;
    }

private:
    const std::string &host;
    const std::string &port;
};


struct AddressInfoCmp {
    template<typename T, typename U>
    bool operator() (const T &lhs, const U &rhs) const noexcept {
        return HostPortRef(lhs) < HostPortRef(rhs);
    }
};

} // namespace coke

#endif // COKE_NSPOLICY_ADDRESS_INFO_H
