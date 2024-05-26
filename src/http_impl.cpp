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

#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <strings.h>
#include <openssl/evp.h>

#include "coke/http_client.h"
#include "coke/http_utils.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

static int encode_auth(const char *p, std::string& auth)
{
    std::size_t len = strlen(p);
    std::size_t base64_len = (len + 2) / 3 * 4;
    char *base64 = (char *)malloc(base64_len + 1);

    if (!base64)
        return -1;

    EVP_EncodeBlock((unsigned char *)base64, (const unsigned char *)p, len);
    auth.append("Basic ");
    auth.append(base64, base64_len);

    free(base64);
    return 0;
}

static std::string_view get_http_host(coke::HttpRequest &req) {
    constexpr int host_len = 4;

    for (const coke::HttpHeaderView &header : coke::HttpHeaderCursor(req)) {
        if (header.name.length() != host_len)
            continue;
        if (strncasecmp(header.name.data(), "Host", host_len) == 0)
            return header.value;
    }
    return std::string_view();
}

HttpClient::AwaiterType
HttpClient::request(const std::string &url, const std::string &method,
                    const HttpHeader &headers, std::string body) {
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
    req.append_output_body(body.data(), body.size());

    for (const auto &pair : headers)
        req.add_header_pair(pair.first, pair.second);

    return create_task(url, &req);
}


HttpClient::AwaiterType
HttpClient::create_task(const std::string &url, ReqType *req) noexcept {
    WFHttpTask *task;
    HttpRequest *treq;
    bool https;
    bool has_proxy = !params.proxy.empty();
    const std::string &proxy = params.proxy;

    https = (strncasecmp(url.c_str(), "https://", 8) == 0);

    // redirect disabled when http url with proxy

    if (has_proxy && https)
        task = WFTaskFactory::create_http_task(url, proxy, params.redirect_max, params.retry_max, nullptr);
    else if (has_proxy)
        task = WFTaskFactory::create_http_task(proxy, 0, params.retry_max, nullptr);
    else
        task = WFTaskFactory::create_http_task(url, params.redirect_max, params.retry_max, nullptr);

    treq = task->get_req();

    if (req) {
        const char *uri = req->get_request_uri();
        const char *turi = treq->get_request_uri();

        // Use request_uri in url if not set in req
        if ((!uri || !*uri) && turi && !has_proxy)
            req->set_request_uri(turi);

        std::string_view req_host, treq_host;
        req_host = get_http_host(*req);
        if (req_host.empty()) {
            treq_host = get_http_host(*treq);
            if (!treq_host.empty())
                req->set_header_pair("Host", std::string(treq_host));
        }

        *treq = std::move(*req);
    }

    // Use simple proxy if not https
    if (has_proxy && !https) {
        ParsedURI proxy_uri;
        if (URIParser::parse(proxy, proxy_uri) == 0) {
            if (proxy_uri.userinfo  && proxy_uri.userinfo[0]) {
                std::string proxy_auth;
                if (encode_auth(proxy_uri.userinfo, proxy_auth) == 0)
                    treq->add_header_pair("Proxy-Authorization", proxy_auth.data());
            }
        }

        treq->set_request_uri(url);
    }

    if (!treq->get_http_version())
        treq->set_http_version("HTTP/1.1");

    if (!treq->get_request_uri())
        treq->set_request_uri("/");

    if (!treq->get_method())
        treq->set_method("GET");

    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);

    return AwaiterType(task);
}

bool HttpHeaderCursor::iterator::next() noexcept {
    if (!cursor.next)
        return false;

    const void *name, *value;
    std::size_t nlen, vlen;

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
