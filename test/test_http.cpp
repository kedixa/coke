#include <gtest/gtest.h>
#include <cerrno>

#include "coke/coke.h"
#include "coke/http_client.h"
#include "coke/http_server.h"

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
