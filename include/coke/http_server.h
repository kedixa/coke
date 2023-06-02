#ifndef COKE_HTTP_SERVER_H
#define COKE_HTTP_SERVER_H

#include "coke/detail/task.h"
#include "coke/network.h"

#include "workflow/WFHttpServer.h"
#include "workflow/HttpMessage.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpAwaiter = NetworkAwaiter<HttpRequest, HttpResponse>;
using HttpServerContext = ServerContext<WFNetworkTask<HttpRequest, HttpResponse>>;

// TODO: The inheritance relationship of server may be modified in the near future
class HttpServer : public WFHttpServer {
    using ProcessorType = std::function<Task<>(HttpServerContext)>;

public:
    HttpServer(const struct WFServerParams *params, ProcessorType co_proc)
        : WFHttpServer(params, std::bind(&HttpServer::do_proc, this, std::placeholders::_1)),
        co_proc(std::move(co_proc))
    { }

    HttpServer(ProcessorType co_proc)
        : HttpServer(&HTTP_SERVER_PARAMS_DEFAULT, std::move(co_proc))
    { }

private:
    void do_proc(WFHttpTask *task) {
        Task<> t = co_proc(HttpServerContext(task));
        t.start_on_series(series_of(task));
    }

private:
    ProcessorType co_proc;
};

} // namespace coke

#endif // COKE_HTTP_SERVER_H
