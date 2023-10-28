#ifndef COKE_HTTP_SERVER_H
#define COKE_HTTP_SERVER_H

#include "coke/basic_server.h"

#include "workflow/WFHttpServer.h"
#include "workflow/HttpMessage.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpServerContext = ServerContext<HttpRequest, HttpResponse>;
using HttpReplyResult = NetworkReplyResult;

struct HttpServerParams : public ServerParams {
    HttpServerParams() : ServerParams(HTTP_SERVER_PARAMS_DEFAULT) { }
    ~HttpServerParams() = default;
};

class HttpServer : public BasicServer<HttpRequest, HttpResponse> {
public:
    HttpServer(const HttpServerParams &params, ProcessorType co_proc)
        : BasicServer<HttpRequest, HttpResponse>(params, std::move(co_proc))
    { }

    HttpServer(ProcessorType co_proc)
        : BasicServer<HttpRequest, HttpResponse>(HttpServerParams(), std::move(co_proc))
    { }
};

} // namespace coke

#endif // COKE_HTTP_SERVER_H
