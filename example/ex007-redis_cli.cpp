#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <getopt.h>

#include "coke/coke.h"
#include "coke/redis_client.h"
#include "coke/redis_utils.h"
#include "readline_helper.h"

std::string prompt_str;
const char *optstr = "h:p:a:r:i:n:";

/**
 * This example implements a simple redis-cli, read commands from standard input,
 * send them to the redis server, and write the results to standard output.
*/

bool get_command(std::string &cmd, std::vector<std::string> &args) {
    using str_iter = std::istream_iterator<std::string>;
    std::string line;

    while (nextline(prompt_str.c_str(), line)) {
        // Only handle space delimiters, just for example
        std::istringstream iss(line);
        iss >> cmd;

        if (!cmd.empty()) {
            args.clear();
            std::copy(str_iter(iss), str_iter{}, std::back_inserter(args));
            add_history(line);
            return true;
        }
    }

    return false;
}

void show_result(const coke::RedisResult &res) {
    if (res.state != coke::STATE_SUCCESS) {
        const char *errstr = coke::get_error_string(res.state, res.error);
        std::cout << "Error: " << errstr << std::endl;
    }
    else {
        coke::RedisValue v;
        res.resp.get_result(v);

        // Show result like redis-cli
        std::cout << coke::redis_value_to_string(v);
        std::cout.flush();
    }
}

coke::Task<> redis_cli(coke::RedisClient &cli, int repeat, long sec, long nsec) {
    coke::RedisResult res;
    std::string cmd;
    std::vector<std::string> args;

    // We'd better not use blocking operations in coroutines,
    // the blocking 'get_command' is just a simple example here.
    while (get_command(cmd, args)) {
        if (strcasecmp(cmd.c_str(), "quit") == 0)
            break;

        for (int i = 0; i < repeat; i++) {
            res = co_await cli.request(cmd, args);
            show_result(res);
            co_await coke::sleep(sec, nsec);
        }
    }
}

void usage(const char *arg0) {
    std::cout << "Usage: " << arg0 << " [options]...\n";
    std::cout << R"(
        -h hostname     Server hostname.
        -p port         Server port, default 6379.
        -a password     Password to use when connecting to the server.
        -n db           Database number.
        -r repeat       Execute N times for each command.
        -i interval     Waits interval seconds after each command.
                        It is possible to specify sub-second times like -i 0.1.
    )" << std::endl;
}

int main(int argc, char *argv[]) {
    coke::RedisClientParams params;
    int repeat = 0, copt;
    long interval_sec = 0, interval_nsec = 0;
    double d = 0.0;

    while ((copt = getopt(argc, argv, optstr)) != -1) {
        switch (copt) {
        case 'h': params.host.assign(optarg); break;
        case 'p': params.port = std::atoi(optarg); break;
        case 'a': params.password.assign(optarg); break;
        case 'n': params.db = std::atoi(optarg); break;
        case 'r': repeat = std::atoi(optarg); break;
        case 'i': d = std::atof(optarg); break;
        default: usage(argv[0]); return 1;
        }
    }

    if (params.host.empty()) {
        usage(argv[0]);
        return 1;
    }

    // Switch interval to seconds and nano seconds
    if (d > 0) {
        interval_sec = static_cast<long>(std::floor(d));
        interval_nsec = static_cast<long>((d - interval_sec) * 1000 * 1000 * 1000);
    }

    if (repeat < 1)
        repeat = 1;

    readline_init();
    prompt_str.assign(params.host).append(":").append(std::to_string(params.port)).append("> ");

    coke::RedisClient cli(params);
    coke::sync_wait(redis_cli(cli, repeat, interval_sec, interval_nsec));

    readline_deinit();

    return 0;
}
