#include "coke/detail/awaiter_base.h"
#include "coke/global.h"
#include "coke/coke.h"

#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <cassert>

#include "workflow/Workflow.h"
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

// Check global constant value are always same as workflow's
static_assert(STATE_UNDEFINED == WFT_STATE_UNDEFINED);
static_assert(STATE_SUCCESS == WFT_STATE_SUCCESS);
static_assert(STATE_TOREPLY == WFT_STATE_TOREPLY);
static_assert(STATE_NOREPLY == WFT_STATE_NOREPLY);
static_assert(STATE_SYS_ERROR == WFT_STATE_SYS_ERROR);
static_assert(STATE_SSL_ERROR == WFT_STATE_SSL_ERROR);
static_assert(STATE_DNS_ERROR == WFT_STATE_DNS_ERROR);
static_assert(STATE_TASK_ERROR == WFT_STATE_TASK_ERROR);
static_assert(STATE_ABORTED == WFT_STATE_ABORTED);
static_assert(NOT_TIMEOUT == TOR_NOT_TIMEOUT);
static_assert(WAIT_TIMEOUT == TOR_WAIT_TIMEOUT);
static_assert(CONNECT_TIMEOUT == TOR_CONNECT_TIMEOUT);
static_assert(TRANSMIT_TIMEOUT == TOR_TRANSMIT_TIMEOUT);


void library_init(const GlobalSettings &s) {
    WFGlobalSettings t = GLOBAL_SETTINGS_DEFAULT;

    t.endpoint_params.max_connections       = s.endpoint_params.max_connections;
    t.endpoint_params.connect_timeout       = s.endpoint_params.connect_timeout;
    t.endpoint_params.response_timeout      = s.endpoint_params.response_timeout;
    t.endpoint_params.ssl_connect_timeout   = s.endpoint_params.ssl_connect_timeout;
    t.endpoint_params.use_tls_sni           = s.endpoint_params.use_tls_sni;

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

void *AwaiterBase::create_series(SubTask *first) {
    return Workflow::create_series_work(first, nullptr);
}

void AwaiterBase::suspend(void *s, bool is_new) {
    SeriesWork *series = static_cast<SeriesWork *>(s);

    if (is_new)
        series->start();
    else if(!in_series)
        series->push_back(subtask);
}

AwaiterBase::~AwaiterBase() {
    // We assume that SubTask can be deleted
    delete subtask;
}

} // namespace coke
