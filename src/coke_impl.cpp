/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
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

#include <atomic>
#include <type_traits>

#include "coke/detail/awaiter_base.h"
#include "coke/detail/mutex_table.h"
#include "coke/detail/constant.h"
#include "coke/detail/exception_config.h"
#include "coke/coke.h"

#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

// Check global constant value are always same as workflow's
static_assert(STATE_UNDEFINED   == WFT_STATE_UNDEFINED);
static_assert(STATE_SUCCESS     == WFT_STATE_SUCCESS);
static_assert(STATE_TOREPLY     == WFT_STATE_TOREPLY);
static_assert(STATE_NOREPLY     == WFT_STATE_NOREPLY);
static_assert(STATE_SYS_ERROR   == WFT_STATE_SYS_ERROR);
static_assert(STATE_SSL_ERROR   == WFT_STATE_SSL_ERROR);
static_assert(STATE_DNS_ERROR   == WFT_STATE_DNS_ERROR);
static_assert(STATE_TASK_ERROR  == WFT_STATE_TASK_ERROR);
static_assert(STATE_ABORTED     == WFT_STATE_ABORTED);

static_assert(CTOR_NOT_TIMEOUT      == TOR_NOT_TIMEOUT);
static_assert(CTOR_WAIT_TIMEOUT     == TOR_WAIT_TIMEOUT);
static_assert(CTOR_CONNECT_TIMEOUT  == TOR_CONNECT_TIMEOUT);
static_assert(CTOR_TRANSMIT_TIMEOUT == TOR_TRANSMIT_TIMEOUT);

#ifdef __cpp_lib_is_layout_compatible
static_assert(std::is_layout_compatible_v<EndpointParams, ::EndpointParams>);
#else
static_assert(sizeof(EndpointParams) == sizeof(::EndpointParams));
#endif

void library_init(const GlobalSettings &s) {
    WFGlobalSettings t = GLOBAL_SETTINGS_DEFAULT;

    t.endpoint_params.address_family        = s.endpoint_params.address_family;
    t.endpoint_params.max_connections       = s.endpoint_params.max_connections;
    t.endpoint_params.connect_timeout       = s.endpoint_params.connect_timeout;
    t.endpoint_params.response_timeout      = s.endpoint_params.response_timeout;
    t.endpoint_params.ssl_connect_timeout   = s.endpoint_params.ssl_connect_timeout;
    t.endpoint_params.use_tls_sni           = s.endpoint_params.use_tls_sni;

    t.dns_server_params.address_family      = s.dns_server_params.address_family;
    t.dns_server_params.max_connections     = s.dns_server_params.max_connections;
    t.dns_server_params.connect_timeout     = s.dns_server_params.connect_timeout;
    t.dns_server_params.response_timeout    = s.dns_server_params.response_timeout;
    t.dns_server_params.ssl_connect_timeout = s.dns_server_params.ssl_connect_timeout;
    t.dns_server_params.use_tls_sni         = s.dns_server_params.use_tls_sni;

    t.dns_ttl_default   = s.dns_ttl_default;
    t.dns_ttl_min       = s.dns_ttl_min;
    t.poller_threads    = s.poller_threads;
    t.handler_threads   = s.handler_threads;
    t.compute_threads   = s.compute_threads;
    t.fio_max_events    = s.fio_max_events;
    t.resolv_conf_path  = s.resolv_conf_path;
    t.hosts_path        = s.hosts_path;

    WORKFLOW_library_init(&t);
}

const char *get_error_string(int state, int error) {
    return WFGlobal::get_error_string(state, error);
}

bool prevent_recursive_stack(bool clear) {
    constexpr std::size_t N = 1024;
    static thread_local std::size_t recursive_count = 0;

    if (clear) {
        recursive_count = 0;
        return false;
    }

    return (++recursive_count) % N == 0;
}

SeriesWork *default_series_creater(SubTask *first) {
    return Workflow::create_series_work(first, nullptr);
}

static SeriesCreater coke_series_creater = default_series_creater;

SeriesCreater set_series_creater(SeriesCreater creater) noexcept {
    SeriesCreater old = coke_series_creater;
    if (creater == nullptr)
        coke_series_creater = default_series_creater;
    else
        coke_series_creater = creater;

    return old;
}

SeriesCreater get_series_creater() noexcept {
    return coke_series_creater;
}

void *AwaiterBase::create_series(SubTask *first) {
    return coke_series_creater(first);
}

uint64_t get_unique_id() noexcept {
    static std::atomic<uint64_t> uid{1};
    // Assume uid will not exhausted before process ends
    return uid.fetch_add(1, std::memory_order_relaxed);
}

void AwaiterBase::suspend(void *s, bool is_new) {
    SeriesWork *series = static_cast<SeriesWork *>(s);

    if (is_new)
        series->start();
    else if(!in_series)
        series->push_front(subtask);
}

AwaiterBase::~AwaiterBase() {
    // We assume that SubTask can be deleted
    delete subtask;
}


namespace detail {

// detail/mutex_table.h impl

struct alignas(DESTRUCTIVE_ALIGN) AlignedMutex {
    std::mutex mtx;
};

std::mutex &get_mutex(const void *ptr) noexcept {
    static AlignedMutex m[MUTEX_TABLE_SIZE];

    uintptr_t h = (uintptr_t)(void *)ptr;
    return m[h%MUTEX_TABLE_SIZE].mtx;
}

// detail/exception_config.h

#ifdef COKE_NO_EXCEPTIONS

void throw_system_error(std::errc) {
    std::terminate();
}

#else // COKE_NO_EXCEPTIONS not defined

void throw_system_error(std::errc e) {
    throw std::system_error(std::make_error_code(e));
}

#endif // COKE_NO_EXCEPTIONS

} // namespace detail

} // namespace coke
