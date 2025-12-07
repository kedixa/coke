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

#ifndef COKE_HTTP_SERVER_H
#define COKE_HTTP_SERVER_H

#include "coke/net/basic_server.h"

#include "workflow/HttpMessage.h"
#include "workflow/WFHttpServer.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpServerContext = ServerContext<HttpRequest, HttpResponse>;
using HttpReplyResult = NetworkReplyResult;

struct HttpServerParams : public ServerParams {
    HttpServerParams() : ServerParams(HTTP_SERVER_PARAMS_DEFAULT) {}
    ~HttpServerParams() = default;
};

class HttpServer : public BasicServer<HttpRequest, HttpResponse> {
public:
    HttpServer(const HttpServerParams &params, ProcessorType co_proc)
        : BasicServer<HttpRequest, HttpResponse>(params, std::move(co_proc))
    {
    }

    HttpServer(ProcessorType co_proc)
        : BasicServer<HttpRequest, HttpResponse>(HttpServerParams(),
                                                 std::move(co_proc))
    {
    }
};

} // namespace coke

#endif // COKE_HTTP_SERVER_H
