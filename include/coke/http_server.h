#ifndef COKE_HTTP_SERVER_H
#define COKE_HTTP_SERVER_H

#include "coke/network.h"
#include "workflow/WFHttpServer.h"
#include "workflow/HttpMessage.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpAwaiter = NetworkAwaiter<HttpRequest, HttpResponse>;
using HttpServerContext = ServerContext<HttpRequest, HttpResponse>;

// TODO: The inheritance relationship of server will be modified in the near future
class HttpServer : public WFHttpServer {
    using processor_t = std::function<Task<>(HttpServerContext ctx)>;

public:
    HttpServer(const struct WFServerParams *params, processor_t co_proc)
        : WFHttpServer(params, std::bind(&HttpServer::do_proc, this, std::placeholders::_1)),
        co_proc(std::move(co_proc))
    { }

    HttpServer(processor_t co_proc) : HttpServer(&HTTP_SERVER_PARAMS_DEFAULT, std::move(co_proc))
    { }

private:
    void do_proc(WFHttpTask *task) {
        Task<> t = co_proc(HttpServerContext(task));
        t.start_on_series(series_of(task));
    }

private:
    processor_t co_proc;
};

} // namespace coke

#endif // COKE_HTTP_SERVER_H
