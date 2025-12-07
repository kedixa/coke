#include <iostream>
#include <string>

#include "coke/http/http_client.h"
#include "coke/http/http_utils.h"
#include "coke/wait.h"

/**
 * This example uses coke::HttpClient to send an Http Get request
 * and print the returned content to standard output.
 */

coke::Task<> http_get(const std::string &url)
{
    coke::HttpClient cli;
    coke::HttpResult res = co_await cli.request(url);

    if (res.state == coke::STATE_SUCCESS) {
        coke::HttpResponse &resp = res.resp;

        std::cout << resp.get_http_version() << ' ' << resp.get_status_code()
                  << ' ' << resp.get_reason_phrase() << "\n\n";

        for (coke::HttpHeaderView h : coke::HttpHeaderCursor(resp))
            std::cout << h.name << ": " << h.value << "\n";

        // In order to ensure a more brief display,
        // the http body is not output to the console.
        std::cout << "\nBody chunked: " << resp.is_chunked() << std::endl;

        if (resp.is_chunked()) {
            for (std::string_view v : coke::HttpChunkCursor(resp))
                std::cout << "Body chunk size: " << v.size() << std::endl;
        }
        else {
            std::string_view body = coke::http_body_view(resp);
            std::cout << "Body size: " << body.size() << "\n";
        }
    }
    else {
        std::cerr << "ERROR: state:" << res.state << " error:" << res.error
                  << std::endl;
        std::cerr << coke::get_error_string(res.state, res.error) << std::endl;
    }
}

// Example: ./http_get http://example.com/
int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " URL" << std::endl;
        std::cerr << "Example: " << argv[0] << " http://example.com/"
                  << std::endl;
        return 1;
    }

    coke::sync_wait(http_get(argv[1]));

    return 0;
}
