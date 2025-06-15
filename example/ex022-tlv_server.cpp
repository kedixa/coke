#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <iostream>

#include "coke/stop_token.h"
#include "coke/tlv/server.h"
#include "coke/tools/option_parser.h"
#include "coke/wait.h"

/**
 * This example shows how to start a TlvServer, and display the current qps.
 */

std::atomic<std::size_t> query_count{0};
std::atomic_flag stop_flag;

void sig_handler(int)
{
    stop_flag.test_and_set();
    stop_flag.notify_all();
}

coke::Task<> show_qps(coke::StopToken &st)
{
    coke::StopToken::FinishGuard guard(&st);
    std::chrono::seconds one_sec(1);
    std::size_t count;

    while (!st.stop_requested()) {
        co_await st.wait_stop_for(one_sec);

        count = query_count.exchange(0);
        std::cout << "TlvServer qps:" << count << std::endl;
    }

    co_return;
}

coke::Task<> process(coke::TlvServerContext ctx)
{
    coke::TlvRequest &req   = ctx.get_req();
    coke::TlvResponse &resp = ctx.get_resp();

    resp.set_type(req.get_type());
    resp.set_value(*req.get_value());

    query_count.fetch_add(1, std::memory_order_relaxed);
    co_await ctx.reply();
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    int port;
    int handler_threads;
    int poller_threads;
    int max_connections;

    coke::OptionParser args;

    args.add_integer(port, 'p', "port")
        .set_default(6789)
        .set_description("The port to listen on.");

    args.add_integer(handler_threads, 't', "handler")
        .set_default(8)
        .set_description("Number of handler threads.");

    args.add_integer(poller_threads, 'P', "poller")
        .set_default(8)
        .set_description("Number of poller threads.");

    args.add_integer(max_connections, 'm', "max-connections")
        .set_default(5000)
        .set_description("Maximum number of client connections.");

    args.set_help_flag(coke::NULL_SHORT_NAME, "help");
    args.set_program("tlv_server");

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
    gs.handler_threads = handler_threads;
    gs.poller_threads  = poller_threads;
    coke::library_init(gs);

    coke::TlvServerParams srv_params;
    srv_params.max_connections = max_connections;

    coke::TlvServer srv(srv_params, process);

    if (srv.start(port) != 0) {
        std::cout << "Start TlvServer failed errno:" << errno << std::endl;
        return 1;
    }

    coke::StopToken st;
    coke::detach(show_qps(st));

    std::cout << "TlvServer start on " << port
              << ". Send SIGINT or SIGTERM to exit." << std::endl;

    stop_flag.wait(false);
    srv.stop();

    st.request_stop();
    coke::sync_wait(st.wait_finish());

    return 0;
}
