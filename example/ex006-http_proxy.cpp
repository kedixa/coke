#include <cstdio>
#include <iostream>
#include <string>

#include "coke/global.h"
#include "coke/http/http_client.h"
#include "coke/http/http_server.h"
#include "workflow/URIParser.h"

coke::HttpClientParams cli_params{
    .retry_max = 2,
    .send_timeout = -1,
    .receive_timeout = -1,
    .keep_alive_timeout = 60 * 1000,
    .redirect_max = 0,
    .proxy = "",
};

coke::HttpClient cli(cli_params);

/**
 * This example implements a simple http proxy, forwards http requests
 * to the target server, and returns the result.
 */
coke::Task<> process(coke::HttpServerContext ctx)
{
    std::string url;
    ParsedURI uri;
    std::string status_code("0");

    coke::HttpRequest &proxy_req = ctx.get_req();
    coke::HttpResponse &proxy_resp = ctx.get_resp();

    // The request format is as described in rfc7230

    url = proxy_req.get_request_uri();
    if (URIParser::parse(url, uri) != 0) {
        status_code.assign("400");
        proxy_resp.set_status_code("400");
        proxy_resp.set_reason_phrase("Bad Request");
        proxy_resp.append_output_body("<html>Bad Url</html>\n");
    }
    else {
        coke::HttpResult result;
        const void *body;
        std::size_t len;

        // Modify the request message into a normal format
        // and send it to the real target
        proxy_req.set_request_uri(uri.path ? uri.path : "/");
        proxy_req.get_parsed_body(&body, &len);
        proxy_req.append_output_body_nocopy(body, len);

        result = co_await cli.request(url, std::move(proxy_req));

        coke::HttpResponse &resp = result.resp;
        if (result.state == coke::STATE_SUCCESS) {
            resp.get_status_code(status_code);

            // Fill the reply message into the proxy reply
            resp.get_parsed_body(&body, &len);
            resp.append_output_body_nocopy(body, len);

            proxy_resp = std::move(resp);
        }
        else {
            status_code.assign("400");
            proxy_resp.set_status_code("404");
            proxy_resp.set_reason_phrase("Not Found");
            proxy_resp.append_output_body("<html>404 Not Found.</html>\n");
        }
    }

    std::cout << "Request " << url << " status_code:" << status_code << "\n";

    // Reply the result to the requester
    coke::HttpReplyResult reply_result;
    reply_result = co_await ctx.reply();

    if (reply_result.state != coke::STATE_SUCCESS) {
        std::cout << "Reply Failed " << url << " state:" << reply_result.state
                  << " error:" << reply_result.error << std::endl;
    }
}

int main(int argc, char *argv[])
{
    int port = 8000;
    coke::HttpServerParams params;
    params.request_size_limit = 8 * 1024 * 1024;

    if (argc > 1 && argv[1])
        port = std::atoi(argv[1]);

    coke::HttpServer server(params, process);
    if (server.start(port) == 0) {
        std::cout << "Start proxy on port " << port << "\nPress Enter to exit"
                  << std::endl;

        std::cin.get();
        server.stop();
    }
    else {
        std::cerr << "Start proxy failed" << std::endl;
        return -1;
    }

    return 0;
}
