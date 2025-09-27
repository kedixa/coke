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

#include <cerrno>
#include <string>

#include <gtest/gtest.h>
#include <netinet/in.h>

#include "coke/coke.h"
#include "coke/http/http_client.h"
#include "coke/http/http_server.h"
#include "workflow/WFTaskFactory.h"

std::string url;

coke::Task<> test_http_client()
{
    coke::HttpClientParams params;
    params.retry_max = 1;

    coke::HttpClient client(params);
    coke::HttpResult res = co_await client.request(url);

    EXPECT_EQ(res.state, coke::STATE_SUCCESS);
    EXPECT_EQ(res.error, 0);
    EXPECT_STREQ(res.resp.get_status_code(), "200");
}

coke::Task<> test_http_task()
{
    WFHttpTask *task;

    task = WFTaskFactory::create_http_task(url, 0, 1, nullptr);
    co_await coke::HttpAwaiter(task, false);

    EXPECT_EQ(task->get_state(), WFT_STATE_SUCCESS);
    EXPECT_EQ(task->get_error(), 0);
    EXPECT_STREQ(task->get_resp()->get_status_code(), "200");
}

TEST(HTTP, http_task)
{
    coke::sync_wait(test_http_task());
}

TEST(HTTP, http_client)
{
    coke::sync_wait(test_http_client());
}

coke::Task<> http_processor(coke::HttpServerContext ctx)
{
    coke::HttpResponse &resp = ctx.get_resp();

    resp.set_status_code("200");
    resp.set_http_version("HTTP/1.1");
    resp.set_header_pair("Server", "Coke HTTP Test Server");

    resp.append_output_body_nocopy("<html>Hello World</html>");

    co_await ctx.reply();
}

int main(int argc, char *argv[])
{
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 4;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    coke::HttpServer server(http_processor);
    if (server.start(0) != 0) {
        EXPECT_EQ(0, 1) << "Server start failed " << errno;
        return -1;
    }

    int port = -1, ret;
    sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);

    server.get_listen_addr((sockaddr *)&addr, &addrlen);

    if (addr.ss_family == AF_INET)
        port = ntohs(((sockaddr_in *)&addr)->sin_port);
    else if (addr.ss_family == AF_INET6)
        port = ntohs(((sockaddr_in6 *)&addr)->sin6_port);

    if (port == -1) {
        EXPECT_EQ(0, 1) << "Unknown address family " << addr.ss_family;
        ret = -1;
    }
    else {
        url.append("http://localhost:").append(std::to_string(port));
        ret = RUN_ALL_TESTS();
    }

    server.stop();
    return ret;
}
