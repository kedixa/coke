#include <atomic>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "coke/tlv/client.h"
#include "coke/tools/option_parser.h"
#include "coke/wait.h"

/**
 * This example shows how to use TlvClient. You can use this example to test
 * performance by applying different parameters, the real time qps is displayed
 * on the server side.
 */

long total;
std::atomic<long> count{0};

coke::Task<> run_client(unsigned cid, coke::TlvClient &cli,
                        std::string_view req_data)
{
    coke::TlvResult res;
    long succ = 0, fail = 0;

    while (count++ < total) {
        res = co_await cli.request(0, req_data);

        if (res.get_state() == coke::STATE_SUCCESS)
            ++succ;
        else
            ++fail;
    }

    std::ostringstream oss;
    oss << "Concurrency " << std::setw(4) << cid << " success " << succ
        << " fail " << fail << "\n";

    std::cout << oss.str();
}

int main(int argc, char *argv[])
{
    std::string host, port;
    int handler_threads;
    int poller_threads;
    int max_connections;
    unsigned concurrent;
    unsigned datalen;

    coke::OptionParser args;

    args.add_string(host, 'h', "host")
        .set_default("127.0.0.1")
        .set_description("The host of TlvServer.");

    args.add_string(port, 'p', "port")
        .set_default("6789")
        .set_description("The port of TlvServer.");

    args.add_integer(handler_threads, 't', "handler")
        .set_default(8)
        .set_description("Number of handler threads.");

    args.add_integer(poller_threads, 'P', "poller")
        .set_default(8)
        .set_description("Number of poller threads.");

    args.add_integer(max_connections, 'm', "max-connections")
        .set_default(1000)
        .set_description("Maximum number of connections to each server.");

    args.add_integer(concurrent, 'c', "concurrent")
        .set_default(4)
        .set_description("Number of concurrent requests.");

    args.add_integer(datalen, 'd', "datalen")
        .set_default(64)
        .set_description("Length of data in each request.");

    args.add_integer(total, 'n', "total")
        .set_default(1000000)
        .set_description("Total number of requests.");

    args.set_help_flag(coke::NULL_SHORT_NAME, "help");
    args.set_program("tlv_client");

    std::string err;
    int ret = args.parse(argc, argv, err);
    if (ret < 0) {
        std::cerr << err << std::endl;
        return 1;
    }
    else if (ret > 0) {
        args.usage(std::cout);
        return 0;
    }

    coke::GlobalSettings gs;
    gs.handler_threads                 = handler_threads;
    gs.poller_threads                  = poller_threads;
    gs.endpoint_params.max_connections = max_connections;

    coke::library_init(gs);

    coke::TlvClientParams cli_params;
    cli_params.host = host;
    cli_params.port = port;

    std::string data;
    data.reserve(datalen);
    for (unsigned i = 0; i < datalen; i++)
        data.push_back(i % 26 + 'a');

    coke::TlvClient cli(cli_params);
    std::vector<coke::Task<>> tasks;
    tasks.reserve(concurrent);

    for (unsigned i = 0; i < concurrent; i++) {
        tasks.emplace_back(run_client(i, cli, data));
    }

    coke::sync_wait(std::move(tasks));

    return 0;
}
