#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>

#include "coke/coke.h"
#include "coke/tools/option_parser.h"
#include "coke/redis_client.h"
#include "coke/redis_utils.h"
#include "readline_helper.h"

std::string prompt_str;

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

coke::Task<> redis_cli(coke::RedisClient &cli, int repeat, double interval) {
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
            co_await coke::sleep(interval);
        }
    }
}

int main(int argc, char *argv[]) {
    coke::RedisClientParams params;
    int repeat = 0;
    double interval = 0.0;

    coke::OptionParser args;

    args.add_string(params.host, 'h', "host", true)
        .set_description("Redis server hostname.");
    args.add_integer(params.port, 'p', "port")
        .set_default(6379)
        .set_description("Redis server port.");
    args.add_string(params.password, 'a', "password")
        .set_description("Password to use when connection to redis server.");
    args.add_integer(params.db, 'n', "database")
        .set_default(0)
        .set_description("Database number.");
    args.add_integer(repeat, 'r', "repeat")
        .set_default(1)
        .set_description("Times to execute for each command.");
    args.add_floating(interval, 'i', "interval")
        .set_default(0.0)
        .set_description("Wait interval seconds after each command.");
    args.set_help_flag(coke::NULL_SHORT_NAME, "help");

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

    if (interval < 0.0)
        interval = 0.0;

    if (repeat < 1)
        repeat = 1;

    readline_init();

    if (params.host.find(':') != std::string::npos)
        prompt_str.append("[").append(params.host).append("]");
    else
        prompt_str.append(params.host);

    if (params.port)
        prompt_str.append(":").append(std::to_string(params.port));

    prompt_str.append("> ");

    coke::RedisClient cli(params);
    coke::sync_wait(redis_cli(cli, repeat, interval));

    readline_deinit();
    return 0;
}
