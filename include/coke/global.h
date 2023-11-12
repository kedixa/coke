#ifndef COKE_GLOBAL_H
#define COKE_GLOBAL_H

#include <cstddef>
#include <cstdint>

namespace coke {

constexpr int MAJOR_VERSION = 0;
constexpr int MINOR_VERSION = 1;
constexpr int BUGFIX_VERSION = 0;

constexpr int STATE_UNDEFINED = -1;
constexpr int STATE_SUCCESS = 0;
constexpr int STATE_TOREPLY = 3;
constexpr int STATE_NOREPLY = 4;
constexpr int STATE_SYS_ERROR = 1;
constexpr int STATE_SSL_ERROR = 65;
constexpr int STATE_DNS_ERROR = 66;
constexpr int STATE_TASK_ERROR = 67;
constexpr int STATE_ABORTED = 2;

constexpr int NOT_TIMEOUT = 0;
constexpr int WAIT_TIMEOUT = 1;
constexpr int CONNECT_TIMEOUT = 2;
constexpr int TRANSMIT_TIMEOUT = 3;


struct EndpointParams {
    size_t  max_connections     = 200;
    int     connect_timeout     = 10 * 1000;
    int     response_timeout    = 10 * 1000;
    int     ssl_connect_timeout = 10 * 1000;
    bool    use_tls_sni         = false;
};

struct GlobalSettings {
    EndpointParams endpoint_params;
    EndpointParams dns_server_params;
    unsigned int dns_ttl_default        = 12 * 3600;
    unsigned int dns_ttl_min            = 180;
    int dns_threads                     = 4;
    int poller_threads                  = 4;
    int handler_threads                 = 20;
    int compute_threads                 = -1;
    int fio_max_events                  = 4096;
    const char *resolv_conf_path        = "/etc/resolv.conf";
    const char *hosts_path              = "/etc/hosts";
};


void library_init(const GlobalSettings &s);
const char *get_error_string(int state, int error);


/**
 * Get a globally unique id, which must be greater than zero,
 * so zero can be treated as an illegal id.
*/
uint64_t get_unique_id();

constexpr uint64_t INVALID_UNIQUE_ID = 0;

} // namespace coke

#endif // COKE_GLOBAL_H
