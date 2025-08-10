#include <cctype>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "coke/redis/server.h"
#include "coke/sleep.h"
#include "coke/task.h"
#include "coke/tools/option_parser.h"

/**
 * This example start a redis server, shows how to use connection context to
 * implement database selection.
 */

constexpr int MAX_DATABASE = 16;
std::unordered_map<std::string, std::string> database[MAX_DATABASE];
std::shared_mutex db_mtx[MAX_DATABASE];

struct ConnInfo {
    int db_idx;
};

int parse_db_index(const std::string &str)
{
    try {
        int i = std::stoi(str);
        if (i < 0 || i >= MAX_DATABASE)
            return -1;
        return i;
    }
    catch (...) {
        return -1;
    }
}

coke::Task<> redis_processor(coke::RedisServerContext ctx)
{
    coke::RedisRequest &req = ctx.get_req();
    coke::RedisResponse &resp = ctx.get_resp();

    coke::StrHolderVec cmd = req.get_command();
    std::string cmd_name;

    if (cmd.empty()) {
        std::string err = "ERROR Bad request";
        resp.set_value(coke::make_redis_simple_error(err));

        // Even without explicit use of co_await ctx.reply(), the message will
        // be returned to the client after the current coroutine returns.
        co_return;
    }

    // The StrHolder from redis server request is string, it is safe to
    // use get_string without check StrHolder::holds_string.

    cmd_name = cmd[0].get_string();
    for (char &c : cmd_name)
        c = std::tolower(c);

    std::cout << "Process command " << cmd_name << std::endl;

    int db_idx;
    WFConnection *conn = ctx.get_task()->get_connection();
    ConnInfo *conn_info = reinterpret_cast<ConnInfo *>(conn->get_context());

    if (conn_info == nullptr) {
        // If conn info is nullptr, set a new one on the connection.
        // The context will be destroyed when the connection is released,
        conn_info = new ConnInfo();

        // Default database is 0.
        conn_info->db_idx = 0;
        conn->set_context(conn_info, [](void *p) {
            delete reinterpret_cast<ConnInfo *>(p);
        });
    }

    if (cmd_name == "select") {
        db_idx = -1;
        if (cmd.size() == 2)
            db_idx = parse_db_index(cmd[1].get_string());

        if (db_idx == -1) {
            std::string err = "ERROR DB index is out of range";
            resp.set_value(coke::make_redis_simple_error(err));

            // Maybe this is a bad client, make it slow.
            co_await coke::sleep(0.5);
            co_return;
        }

        conn_info->db_idx = db_idx;
        resp.set_value(coke::make_redis_simple_string("OK"));
        co_return;
    }
    else
        db_idx = conn_info->db_idx;

    // Successfully get db from conn_info, continue processing the request

    coke::RedisValue value;
    std::shared_mutex &mtx = db_mtx[db_idx];
    auto &db = database[db_idx];

    if (cmd_name == "get" && cmd.size() == 2) {
        std::shared_lock<std::shared_mutex> lg(mtx);
        auto it = db.find(cmd[1].get_string());

        if (it == db.end())
            value.set_null();
        else
            value.set_bulk_string(it->second);
    }
    else if (cmd_name == "set" && cmd.size() == 3) {
        std::unique_lock<std::shared_mutex> lg(mtx);
        db[cmd[1].get_string()] = cmd[2].get_string();
        value.set_simple_string("OK");
    }
    else if (cmd_name == "del" && cmd.size() == 2) {
        std::unique_lock<std::shared_mutex> lg(mtx);
        auto it = db.find(cmd[1].get_string());
        value.set_integer(it == db.end() ? 0 : 1);

        if (it != db.end())
            db.erase(it);
    }
    else
        value.set_simple_error("ERR unknown command");

    ctx.get_resp().set_value(std::move(value));

    // The reply will fail if the client disconnects before replying.
    // Check the state and error after ctx.reply().
    auto reply_result = co_await ctx.reply();

    std::cout << "Db:" << db_idx << " Command:" << cmd_name
              << " State:" << reply_result.state
              << " Error:" << reply_result.error << std::endl;
}

int main(int argc, char *argv[])
{
    coke::OptionParser args;
    int port = 6379;

    args.add_integer(port, 'p', "port")
        .set_default(6379)
        .set_description("Start server at this port.");
    args.set_help_flag('h', "help");

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

    coke::RedisServer server(redis_processor);

    if (server.start(port) == 0) {
        std::cout << "RedisServer start on " << port << "\n"
                  << "Press Enter to exit" << std::endl;

        std::cin.get();
        server.stop();
    }
    else {
        std::cerr << "RedisServer start failed errno:" << errno << std::endl;
    }

    return 0;
}
