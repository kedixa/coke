#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>

#include <sys/un.h>

#include "coke/tlv/client.h"
#include "coke/tlv/server.h"
#include "coke/wait.h"

/**
 * This example shows how to use Unix domain socket for client-server
 * communication.
 *
 * Unlike TlvClient, the TlvConnectionClient does not support concurrency, but
 * ensures each request is sent ovet the same TCP connection unless the
 * connection is disconnected.
 */

coke::Task<> process(coke::TlvServerContext ctx)
{
    coke::TlvRequest &req   = ctx.get_req();
    coke::TlvResponse &resp = ctx.get_resp();

    std::string *val = req.get_value();
    std::string resp_value;

    if (*val == "disconnect") {
        // Simulate disconnected by server after this request.
        resp_value.assign("Disconnecting ...");
        ctx.get_task()->set_keep_alive(0);
    }
    else {
        resp_value.assign("Hello tlv client, your value is: ");
        resp_value.append(*val);
    }

    resp.set_type(req.get_type());
    resp.set_value(resp_value);

    co_await ctx.reply();
}

coke::Task<> run_client(coke::TlvConnectionClient &cli)
{
    co_await coke::yield();
    std::cout << "Input some message to talk with tlv server.\n"
              << "Input quit to exit.\n"
              << "Input disconnect to simulate an unexpected disconnection,\n"
              << "the next request will be failed to notify the event,\n"
              << "another connection will be created if try again.\n"
              << std::endl
              << "Input message: ";

    coke::TlvResult res;
    std::string data;
    int state;

    while (std::getline(std::cin, data)) {
        if (data == "quit")
            break;

        res   = co_await cli.request(0, data);
        state = res.get_state();

        if (state != coke::STATE_SUCCESS) {
            std::cerr << "Request failed with state " << state << " error "
                      << res.get_error() << std::endl;
        }
        else {
            std::cout << "Server: " << res.get_value() << std::endl;
        }

        co_await coke::yield();
        std::cout << "Input message: ";
    }

    res = co_await cli.disconnect();
    if (res.get_state() != coke::STATE_SUCCESS) {
        std::cerr << "Disconnect failed with state " << state << " error "
                  << res.get_error() << std::endl;
    }
}

int main()
{
    sockaddr_storage addr_storage;
    sockaddr_un *addr;

    std::memset(&addr_storage, 0, sizeof(addr_storage));
    addr             = (sockaddr_un *)&addr_storage;
    addr->sun_family = AF_UNIX;

    // Use abstract namespace socket in case we do not have write permission,
    // remove the prefix '\0' to use a normal one.
    char uds_path[] = "\0coke-tlv-server.sock";
    std::memcpy(addr->sun_path, uds_path, sizeof(uds_path));

    coke::TlvServerParams srv_params;
    coke::TlvServer srv(srv_params, process);

    int ret = srv.start((sockaddr *)addr, sizeof(sockaddr_un));
    if (ret < 0) {
        std::cerr << "Start TlvServer failed errno:" << errno << std::endl;
        return 1;
    }

    coke::TlvClientParams cli_params = {
        .transport_type = TT_TCP,
        .addr_storage   = addr_storage,
        .addr_len       = sizeof(sockaddr_un),
    };

    coke::TlvConnectionClient cli(cli_params);
    coke::sync_wait(run_client(cli));

    srv.stop();

    return 0;
}
