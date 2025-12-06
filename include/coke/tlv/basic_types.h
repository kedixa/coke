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

#ifndef COKE_TLV_BASIC_TYPES_H
#define COKE_TLV_BASIC_TYPES_H

#include <cstdint>

#include "coke/net/client_conn_info.h"
#include "workflow/TLVMessage.h"
#include "workflow/WFTask.h"

namespace coke {

using TlvRequest = protocol::TLVRequest;
using TlvResponse = protocol::TLVResponse;

using TlvTask = WFNetworkTask<TlvRequest, TlvResponse>;
using tlv_callback_t = std::function<void(TlvTask *)>;

enum _tlv_err : int {
    TLV_ERR_CLI_INFO = 1,
    TLV_ERR_AUTH = 2,
};

struct TlvClientInfo {
    bool enable_auth;
    int32_t auth_type;
    int32_t auth_success_type;
    std::string auth_value;

    // Attention: Update full_info when TlvClientInfo has new members.

    ClientConnInfo conn_info;
};

class TlvResult {
public:
    TlvResult() = default;

    TlvResult(const TlvResult &) = default;
    TlvResult(TlvResult &&) = default;

    TlvResult &operator=(const TlvResult &) = default;
    TlvResult &operator=(TlvResult &&) = default;

    void set_state(int state) { this->state = state; }
    void set_error(int error) { this->error = error; }
    void set_type(int32_t type) { this->type = type; }
    void set_value(std::string value) { this->value = std::move(value); }

    int get_state() const { return state; }
    int get_error() const { return error; }
    int32_t get_type() const { return type; }

    const std::string &get_value() const & { return value; }
    std::string get_value() && { return std::move(value); }

private:
    int state{0};
    int error{0};

    int32_t type{0};
    std::string value;
};

} // namespace coke

#endif // COKE_TLV_BASIC_TYPES_H
