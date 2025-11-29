#include <iostream>
#include <string>
#include <vector>

#include "coke/http/http_client.h"
#include "coke/http/http_utils.h"
#include "coke/wait.h"

/**
 * This example send multiple http requests, waits for the results
 * and outputs them to stdout, showing how to start a group of tasks
 * in parallel.
 */

coke::Task<> parallel_http_get(std::vector<std::string> urls)
{
    coke::HttpClient cli;
    std::vector<coke::HttpResult> results;
    std::vector<coke::HttpAwaiter> tasks;
    tasks.reserve(urls.size());

    for (const std::string &url : urls)
        tasks.push_back(cli.request(url));

    results = co_await coke::async_wait(std::move(tasks));

    for (std::size_t i = 0; i < urls.size(); i++) {
        auto &res = results[i];

        std::cout << "URL: " << urls[i] << "\n";

        if (res.state == coke::STATE_SUCCESS) {
            coke::HttpResponse &resp = res.resp;

            std::cout << resp.get_http_version() << ' '
                      << resp.get_status_code() << ' '
                      << resp.get_reason_phrase() << "\n\n";

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
            std::cout << "ERROR: state:" << res.state << " error:" << res.error
                      << std::endl;
            std::cout << coke::get_error_string(res.state, res.error)
                      << std::endl;
        }

        std::cout << std::string(80, '-') << std::endl;
    }
}

// Example: ./parallel_http_get http://example.com/ http://example.com/notfound
int main(int argc, char *argv[])
{
    std::vector<std::string> urls;
    for (int i = 1; i < argc; i++)
        urls.emplace_back(argv[i]);

    coke::sync_wait(parallel_http_get(urls));

    return 0;
}
