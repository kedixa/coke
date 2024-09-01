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

#ifndef COKE_GLOBAL_H
#define COKE_GLOBAL_H

#include <cstddef>
#include <cstdint>

namespace coke {

// version
constexpr int COKE_MAJOR_VERSION = 0;
constexpr int COKE_MINOR_VERSION = 3;
constexpr int COKE_PATCH_VERSION = 0;

constexpr const char COKE_VERSION_STR[] = "0.3.0";

// state constant from workflow, see WFTask.h
constexpr int STATE_UNDEFINED = -1;
constexpr int STATE_SUCCESS = 0;
constexpr int STATE_TOREPLY = 3;
constexpr int STATE_NOREPLY = 4;
constexpr int STATE_SYS_ERROR = 1;
constexpr int STATE_SSL_ERROR = 65;
constexpr int STATE_DNS_ERROR = 66;
constexpr int STATE_TASK_ERROR = 67;
constexpr int STATE_ABORTED = 2;

// timeout reason constant from workflow, see CommRequest.h
// `CTOR` means `coke timeout reason`
constexpr int CTOR_NOT_TIMEOUT = 0;
constexpr int CTOR_WAIT_TIMEOUT = 1;
constexpr int CTOR_CONNECT_TIMEOUT = 2;
constexpr int CTOR_TRANSMIT_TIMEOUT = 3;

// return value of coroutine operation such as Mutex::lock
// Negative numbers indicate system errors

// The operation success
constexpr int TOP_SUCCESS = 0;
// The operation timeout before success, used by most time-related operations
// such as Mutex::try_lock_for etc.
constexpr int TOP_TIMEOUT = 1;
// TOP_ABORTED only appears when the process is about to exit but there are
// still coroutines performing time-related operations, but Coke recommends
// waiting for all coroutines to end before the end of the main function to
// avoid this situation.
constexpr int TOP_ABORTED = 2;
// TOP_CLOSED is used when using an asynchronous container to indicate that the
// current operation failed because the container has been closed.
constexpr int TOP_CLOSED = 3;


struct EndpointParams {
    std::size_t max_connections = 200;
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
 * @brief Get a globally unique id, which must be greater than zero,
 *        so zero can be treated as an illegal id.
*/
uint64_t get_unique_id();

/**
 * @brief Invalid unique id.
*/
constexpr uint64_t INVALID_UNIQUE_ID = 0;

/**
 * Some coroutines do not call asynchronous procedures, but perform synchronous
 * operations and then co_return. If do that all the time in a thread, some
 * compilers may implement it as a recursive operation. Call this function
 * where this situation may occur, if returns true, manually switch the thread
 * with coke::yield(), or other async operations.
 *
 * @param clear Reset recursive count to zero and return false.
*/
bool prevent_recursive_stack(bool clear = false);

} // namespace coke

#endif // COKE_GLOBAL_H
