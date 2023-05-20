#include <iostream>
#include <string>
#include <cerrno>

#include "coke/coke.h"
#include "coke/http_server.h"

/**
 * This example start an http server on port 8888, the server replies a simple
 * http response for each request.
*/

int main() {
    coke::HttpServer server([](coke::HttpServerContext ctx) -> coke::Task<> {
        coke::HttpResponse &resp = ctx.get_resp();

        resp.append_output_body("<html><body>Thanks for using coke!</body></html>");

        // After setting data to resp, await reply to send it to requester.
        // ctx.reply should be awaited exactly once.
        co_await ctx.reply();
    });

    int port = 8888;
    if (server.start(port) == 0) {
        std::cout << "HttpServer start on " << port << "\n"
                  << "Press Enter to exit" << std::endl;

        std::cin.get();
        server.stop();
    }
    else {
        std::cerr << "HttpServer start failed errno:" << errno << std::endl;
    }

    return 0;
}
