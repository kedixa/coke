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

#include "workflow/WFDnsResolver.h"
#include "workflow/WFTaskError.h"

#include "coke/nspolicy/nspolicy.h"

namespace coke {

class TracingData {
public:
    explicit TracingData(NSPolicy *p)
        : policy(p), prev_success(false), prev_notified(false)
    {
    }

    void add_addr(AddressInfo *addr)
    {
        prev_success  = false;
        prev_notified = false;
        history.push_back(addr);
    }

    const NSPolicy::SelectHistory &get_history() const { return history; }

    void set_prev_state(bool success) { prev_success = success; }

    void notify_addr()
    {
        if (!prev_notified) {
            AddressInfo *addr = history.back();
            if (prev_success)
                policy->addr_success(addr);
            else
                policy->addr_failed(addr);

            prev_notified = true;
        }
    }

    ~TracingData()
    {
        notify_addr();

        for (AddressInfo *addr : history)
            addr->dec_ref();
    }

private:
    NSPolicy *policy;
    bool prev_success;
    bool prev_notified;
    NSPolicy::SelectHistory history;
};

static void tracing_deleter(void *data)
{
    delete (TracingData *)data;
}

class BasicRouterTask : public WFResolverTask {
public:
    BasicRouterTask(NSPolicy *policy, const WFNSParams *params,
                    router_callback_t &&callback)
        : WFResolverTask(params, std::move(callback)), policy(policy)
    {
    }

protected:
    virtual void dispatch() override
    {
        if (!policy)
            return WFResolverTask::dispatch();

        ParsedURI &ns_uri         = ns_params_.uri;
        WFNSTracing *tracing      = ns_params_.tracing;
        TracingData *tracing_data = (TracingData *)(tracing->data);
        AddressInfo *addr;

        if (!tracing_data)
            addr = policy->select_address(ns_uri, {});
        else {
            tracing_data->notify_addr();
            addr = policy->select_address(ns_uri, tracing_data->get_history());
        }

        if (!addr) {
            this->state = WFT_STATE_TASK_ERROR;
            this->error = WFT_ERR_UPSTREAM_UNAVAILABLE;
            this->subtask_done();
            return;
        }

        if (!tracing_data) {
            tracing_data     = new TracingData(policy);
            tracing->data    = tracing_data;
            tracing->deleter = tracing_deleter;
        }

        tracing_data->add_addr(addr);

        const std::string &host = addr->get_host();
        const std::string &port = addr->get_port();

        if (!host.empty()) {
            free(ns_uri.host);
            ns_uri.host = strdup(host.c_str());
        }

        if (!port.empty()) {
            free(ns_uri.port);
            ns_uri.port = strdup(port.c_str());
        }

        const AddressParams &addr_params = addr->get_addr_params();

        ep_params_       = addr_params.endpoint_params;
        dns_ttl_default_ = addr_params.dns_ttl_default;
        dns_ttl_min_     = addr_params.dns_ttl_min;

        policy = nullptr;

        return WFResolverTask::dispatch();
    }

private:
    NSPolicy *policy;
};

// NSPolicy impl

NSPolicy::~NSPolicy()
{
    AddressInfo *addr;

    while (!addr_set.empty()) {
        addr = addr_set.front();
        addr_set.erase(addr);
        addr->dec_ref();
    }

    recover_list.clear_unsafe();
}

WFRouterTask *NSPolicy::create_router_task(const WFNSParams *params,
                                           router_callback_t callback)
{
    return new BasicRouterTask(this, params, std::move(callback));
}

void NSPolicy::success(RouteManager::RouteResult *result, WFNSTracing *tracing,
                       CommTarget *target)
{
    auto tracing_data = (TracingData *)(tracing->data);
    tracing_data->set_prev_state(true);
    return WFNSPolicy::success(result, tracing, target);
}

void NSPolicy::failed(RouteManager::RouteResult *result, WFNSTracing *tracing,
                      CommTarget *target)
{
    auto tracing_data = (TracingData *)(tracing->data);
    tracing_data->set_prev_state(false);
    return WFNSPolicy::failed(result, tracing, target);
}

AddressInfo *NSPolicy::get_address(const std::string &host,
                                   const std::string &port) const
{
    AddressInfo *addr = nullptr;
    AddrSetLock addr_set_lk(addr_set_mtx);

    auto it = addr_set.find(HostPortRef{host, port});
    if (it != addr_set.cend()) {
        addr = it.remove_const().get_pointer();
        addr->inc_ref();
    }

    return addr;
}

std::vector<AddressPack> NSPolicy::get_all_address() const
{
    std::vector<AddressPack> v;

    AddrSetLock addr_set_lk(addr_set_mtx);
    v.reserve(addr_set.size());

    for (const AddressInfo &addr : addr_set) {
        v.emplace_back(AddressPack{addr.get_state(), addr.get_host(),
                                   addr.get_port(), addr.get_addr_params()});
    }

    return v;
}

std::vector<bool> NSPolicy::add_addresses(const std::vector<AddressPack> &addrs,
                                          bool replace)
{
    std::vector<bool> v;
    v.reserve(addrs.size());

    for (const auto &p : addrs)
        v.push_back(add_address(p.host, p.port, p.params, replace));

    return v;
}

std::vector<bool>
NSPolicy::remove_addresses(const std::vector<HostPortPack> &addrs)
{
    std::vector<bool> v;
    v.reserve(addrs.size());

    for (const auto &p : addrs)
        v.push_back(remove_address(p.host, p.port));

    return v;
}

std::vector<bool>
NSPolicy::remove_addresses(const std::vector<AddressPack> &addrs)
{
    std::vector<HostPortPack> packs;
    for (const auto &p : addrs)
        packs.emplace_back(HostPortPack{p.host, p.port});

    return remove_addresses(packs);
}

void NSPolicy::add_to_recover_list(AddressInfo *addr)
{
    if (recover_list.empty())
        next_recover_time = addr->recover_at_time;

    recover_list.push_back(addr);
}

void NSPolicy::remove_from_recover_list(AddressInfo *addr)
{
    bool need_reset = (addr == recover_list.front());

    recover_list.erase(addr);

    if (need_reset) {
        if (recover_list.empty())
            next_recover_time = INF_RECOVER_TIME;
        else
            next_recover_time = recover_list.front()->recover_at_time;
    }
}

void NSPolicy::try_recover(bool all_break)
{
    if (recover_list.empty())
        return;

    int64_t recover_before_ms = steady_milliseconds();
    if (params.fast_recover && all_break) {
        // do fast recover when all break
        AddressInfo *addr = recover_list.front();
        if (addr->recover_at_time <= recover_before_ms)
            recover_before_ms = INF_RECOVER_TIME;
    }

    while (!recover_list.empty()) {
        AddressInfo *addr = recover_list.front();
        if (addr->recover_at_time > recover_before_ms)
            break;

        recover_list.pop_front();

        addr->set_state(ADDR_STATE_GOOD);
        addr->fail_marks      = 0;
        addr->first_fail_time = 0;
        addr->recover_at_time = 0;

        recover_addr(addr);
    }

    if (recover_list.empty())
        next_recover_time = INF_RECOVER_TIME;
    else
        next_recover_time = recover_list.front()->recover_at_time;
}

} // namespace coke
