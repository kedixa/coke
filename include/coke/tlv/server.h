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

#ifndef COKE_TLV_SERVER_H
#define COKE_TLV_SERVER_H

#include "coke/net/basic_server.h"
#include "coke/tlv/basic_types.h"

namespace coke {

using TlvServerContext = ServerContext<TlvRequest, TlvResponse>;

constexpr ServerParams TLV_SERVER_PARAMS_DEFAULT = {
    .transport_type        = TT_TCP,
    .max_connections       = 2000,
    .peer_response_timeout = 10 * 1000,
    .receive_timeout       = -1,
    .keep_alive_timeout    = 60 * 1000,
    .request_size_limit    = (size_t)-1,
    .ssl_accept_timeout    = 5000,
};

struct TlvServerParams : public ServerParams {
    TlvServerParams() : ServerParams(TLV_SERVER_PARAMS_DEFAULT) {}
    ~TlvServerParams() = default;
};

class TlvServer : public BasicServer<TlvRequest, TlvResponse> {
    using Base = BasicServer<TlvRequest, TlvResponse>;

public:
    TlvServer(const TlvServerParams &params, ProcessorType co_proc)
        : Base(params, std::move(co_proc))
    {
    }

    TlvServer(ProcessorType co_proc)
        : Base(TLV_SERVER_PARAMS_DEFAULT, std::move(co_proc))
    {
    }
};

} // namespace coke

#endif // COKE_TLV_SERVER_H
