#ifndef COKE_HTTP_CLIENT_H
#define COKE_HTTP_CLIENT_H

#include <vector>
#include <utility>
#include <string>

#include "coke/network.h"

#include "workflow/HttpMessage.h"

namespace coke {

using HttpRequest = protocol::HttpRequest;
using HttpResponse = protocol::HttpResponse;
using HttpAwaiter = NetworkAwaiter<HttpRequest, HttpResponse>;
using HttpResult = HttpAwaiter::ResultType;

struct HttpClientParams {
    int retry_max           = 0;
    int send_timeout        = -1;
    int receive_timeout     = -1;
    int keep_alive_timeout  = 60 * 1000;

    int redirect_max        = 0;
    std::string proxy;
};


class HttpClient {
public:
    using ReqType = HttpRequest;
    using RespType = HttpResponse;
    using AwaiterType = HttpAwaiter;
    using HttpHeader = std::vector<std::pair<std::string, std::string>>;

public:
    explicit
    HttpClient(const HttpClientParams &params = HttpClientParams())
        : params(params)
    { }

    virtual ~HttpClient() = default;

    [[nodiscard]]
    AwaiterType request(const std::string &url) {
        return create_task(url, nullptr);
    }

    [[nodiscard]]
    AwaiterType request(const std::string &url, ReqType &&req) {
        return create_task(url, &req);
    }

    [[nodiscard]]
    AwaiterType request(const std::string &url, const std::string &method,
                        const HttpHeader &headers, std::string body);

protected:
    virtual AwaiterType create_task(const std::string &url, ReqType *req) noexcept;

protected:
    HttpClientParams params;
};

} // namespace coke

#endif // COKE_HTTP_CLIENT_H
