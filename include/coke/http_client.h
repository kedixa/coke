#ifndef COKE_HTTP_CLIENT_H
#define COKE_HTTP_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/basic_client.h"
#include "workflow/HttpMessage.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpResult = NetworkResult<HttpResponse>;

struct HttpClientParams : public ClientParams {
    int redirect_max = 0;
    std::string proxy;
};


class HttpClient : public BasicClient<HttpRequest, HttpResponse> {
    using Base = BasicClient<HttpRequest, HttpResponse>;

public:
    using HttpAwaiter = Base::awaiter_t;
    using HttpTask = Base::task_t;
    using HttpHeader = std::vector<std::pair<std::string, std::string>>;
    using Base::request;

public:
    HttpClient(const HttpClientParams &p = HttpClientParams()) : BasicClient(p) {
        redirect_max = p.receive_timeout;
        proxy = p.proxy;
    }

    HttpAwaiter request(const std::string &method, const std::string &url,
                        const HttpHeader &headers, std::string_view body);

protected:
    HttpTask *create_task(const std::string &url, req_t *req) override;

private:
    int redirect_max{0};
    std::string proxy;
};

} // namespace coke

#endif // COKE_HTTP_CLIENT_H
