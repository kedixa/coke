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
using HttpResult = NetworkResult<HttpResponse>;

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
    using AwaiterType = NetworkAwaiter<ReqType, RespType>;
    using HttpHeader = std::vector<std::pair<std::string, std::string>>;

public:
    explicit
    HttpClient(const HttpClientParams &params = HttpClientParams())
        : params(params)
    { }

    virtual ~HttpClient() = default;

    AwaiterType request(std::string url) {
        return create_task(std::move(url), nullptr);
    }

    AwaiterType request(std::string url, ReqType &&req) {
        return create_task(std::move(url), &req);
    }

    AwaiterType request(std::string url, std::string method,
                        const HttpHeader &headers, std::string body);

protected:
    virtual AwaiterType create_task(std::string url, ReqType *req) noexcept;

protected:
    HttpClientParams params;
};

} // namespace coke

#endif // COKE_HTTP_CLIENT_H
