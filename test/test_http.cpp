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
#include <gtest/gtest.h>

#include "coke/coke.h"
#include "coke/http/http_client.h"
#include "coke/http/http_server.h"

#include "workflow/WFTaskFactory.h"

int http_port = -1;

std::string get_url() {
    std::string url;

    url.assign("http://localhost:")
        .append(std::to_string(http_port))
        .append("/hello");

    return url;
}

coke::Task<> test_http_client() {
    coke::HttpClient client;
    std::string url = get_url();

    coke::HttpResult res = co_await client.request(url);

    EXPECT_EQ(res.state, coke::STATE_SUCCESS);
    EXPECT_EQ(res.error, 0);
    EXPECT_STREQ(res.resp.get_status_code(), "200");
}

coke::Task<> test_http_task() {
    WFHttpTask *task = WFTaskFactory::create_http_task(get_url(), 0, 0, nullptr);
    co_await coke::HttpAwaiter(task, false);

    EXPECT_EQ(task->get_state(), WFT_STATE_SUCCESS);
    EXPECT_EQ(task->get_error(), 0);
    EXPECT_STREQ(task->get_resp()->get_status_code(), "200");
}

TEST(HTTP, http_client) {
    coke::sync_wait(test_http_client());
}

TEST(HTTP, http_task) {
    coke::sync_wait(test_http_task());
}

coke::Task<> http_processor(coke::HttpServerContext ctx) {
    coke::HttpResponse &resp = ctx.get_resp();

    resp.set_status_code("200");
    resp.set_http_version("HTTP/1.1");
    resp.set_header_pair("Server", "Coke HTTP Test Server");

    resp.append_output_body_nocopy("<html>Hello World</html>");

    co_await ctx.reply();
}

int main(int argc, char *argv[]) {
    coke::GlobalSettings s;
    s.poller_threads = 2;
    s.handler_threads = 4;
    s.compute_threads = 2;
    coke::library_init(s);

    testing::InitGoogleTest(&argc, argv);

    coke::HttpServer server(http_processor);

    for (int i = 8000; i < 8010; i++) {
        if (server.start(i) == 0) {
            http_port = i;
            break;
        }
    }

    if (http_port == -1) {
        EXPECT_NE(http_port, -1) << "Server start failed " << errno;
        return -1;
    }

    int ret = RUN_ALL_TESTS();
    server.stop();

    return ret;
}
