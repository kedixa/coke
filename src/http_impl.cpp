#include <cstdint>
#include <cerrno>
#include <cstdlib>

#include "coke/http_client.h"
#include "coke/http_utils.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

HttpClient::HttpAwaiter
HttpClient::request(const std::string &method, const std::string &url,
                    const HttpHeader &headers, std::string_view body) {
    ParsedURI uri;
    HttpRequest req;
    std::string request_uri("/");

    if (URIParser::parse(url, uri) == 0) {
        if (uri.path && uri.path[0])
            request_uri.assign(uri.path);

        if (uri.query && uri.query[0])
            request_uri.append("?").append(uri.query);
    }

    req.set_method(method);
    req.set_request_uri(request_uri);
    req.append_output_body_nocopy(body.data(), body.size());

    for (const auto &pair : headers)
        req.add_header_pair(pair.first, pair.second);

    return Base::request(url, std::move(req));
}


HttpClient::HttpTask *
HttpClient::create_task(const std::string &url, req_t *req) {
    WFHttpTask *task;
    HttpRequest *treq;
    bool has_proxy = !proxy.empty();
    bool https = url.compare(0, 8, "https://", 8) == 0;

    if (has_proxy && https)
        task = WFTaskFactory::create_http_task(url, proxy, redirect_max, params.retry_max, nullptr);
    else if (has_proxy)
        task = WFTaskFactory::create_http_task(proxy, redirect_max, params.retry_max, nullptr);
    else
        task = WFTaskFactory::create_http_task(url, redirect_max, params.retry_max, nullptr);

    treq = task->get_req();

    if (req)
        *treq = std::move(*req);

    // use simple proxy if not https
    if (has_proxy && !https)
        treq->set_request_uri(url);

    if (!treq->get_http_version())
        treq->set_http_version("HTTP/1.1");

    if (!treq->get_request_uri())
        treq->set_request_uri("/");

    if (!treq->get_method())
        treq->set_method("GET");

    return task;
}

bool HttpHeaderCursor::iterator::next() noexcept {
    if (!cursor.next)
        return false;

    const void *name, *value;
    size_t nlen, vlen;

    if (http_header_cursor_next(&name, &nlen, &value, &vlen, &cursor) == 0) {
        data.name = std::string_view(static_cast<const char *>(name), nlen);
        data.value = std::string_view(static_cast<const char *>(value), vlen);
    }
    else {
        cursor.head = nullptr;
        cursor.next = nullptr;
    }

    return true;
}

bool HttpChunkCursor::iterator::next() noexcept {
    if (cur == end) {
        cur = end = nullptr;
        return false;
    }

    if (!chunked) {
        chunk = std::string_view(cur, end);
        cur = end;
        return true;
    }

    char *str_end;
    unsigned long long len;

    errno = 0;
    len = std::strtoull(cur, &str_end, 16);
    if (errno == ERANGE || len == 0 || str_end >= end) {
        cur = end = nullptr;
        return false;
    }

    cur = str_end;
    while (cur < end && *cur != '\r')
        ++cur;

    if (cur + len + 4 > end) {
        cur = end = nullptr;
        return false;
    }

    chunk = std::string_view(cur+2, len);
    cur += len + 4;
    return true;
}

} // namespace coke
