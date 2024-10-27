#include <iostream>
#include <string>
#include <cerrno>
#include <chrono>
#include <unordered_map>
#include <cctype>
#include <shared_mutex>

#include "coke/task.h"
#include "coke/sleep.h"
#include "coke/redis/redis_server.h"

/**
 * This example start a redis server on port 8379, shows how to use
 * connection context to save information by implementing operations
 * such as redis authentication and database selection. 
*/

constexpr int MAX_DATABASE = 16;
std::unordered_map<std::string, std::string> database[MAX_DATABASE];
std::shared_mutex db_mtx[MAX_DATABASE];

const std::unordered_map<std::string, std::string> user_pass = {
    {"",            "coke"},    // default user
    {"kedixa",      "hello"},
    {"workflow",    "world"},
};

struct ConnInfo {
    int db_idx;
    std::string username;
};

enum {
    UPDATE_INFO_SUCCESS = 0,
    UPDATE_INFO_FAILURE = 1,
    CONTINUE_PROCESS = 2,
};

// The helper functions are not the focus of this example, so it is implemented at the end.

void set_error(coke::RedisResponse &resp, const std::string &errmsg);
void set_status(coke::RedisResponse &resp, const std::string &status);
int parse_db_index(const std::string &params);
bool check_user_pass(const std::vector<std::string> &params, std::string &username);

int update_info(coke::RedisServerContext &ctx, ConnInfo &info,
                const std::string &cmd, const std::vector<std::string> &params)
{
    // Get information from connection context, if it has not been set on the current
    // connection before, you will get a null pointer.
    WFConnection *conn = ctx.get_task()->get_connection();
    ConnInfo *conn_info = reinterpret_cast<ConnInfo *>(conn->get_context());

    if (cmd == "auth") {
        std::string username;

        // Auth failed, clear the info on the current connection, and return failure
        if (!check_user_pass(params, username)) {
            delete conn_info;
            conn->set_context(nullptr, nullptr);
            set_error(ctx.get_resp(), "ERR Invalid user");
            return UPDATE_INFO_FAILURE;
        }

        // Auth success

        if (conn_info == nullptr) {
            // If conn info is nullptr, set a new one on the connection.
            // The context will be destroyed when the connection is released,
            // we will discuss this in detail in the documentation, this is why
            // we need to make a copy of ConnInfo when return CONTINUE_PROCESS.
            conn_info = new ConnInfo;
            conn->set_context(conn_info, [](void *p) { delete reinterpret_cast<ConnInfo *>(p); });
        }

        conn_info->username = username;
        conn_info->db_idx = 0;
        set_status(ctx.get_resp(), "OK");
        return UPDATE_INFO_SUCCESS;
    }

    // There is no auth info on the connection, return failure
    if (conn_info == nullptr) {
        set_error(ctx.get_resp(), "NOAUTH Authentication required");
        return UPDATE_INFO_FAILURE;
    }

    // The database being used is also the information saved on the connection.
    if (cmd == "select") {
        int idx = -1;
        if (params.size() == 1)
            idx = parse_db_index(params[0]);

        if (idx == -1) {
            set_error(ctx.get_resp(), "ERR DB index is out of range");
            return UPDATE_INFO_FAILURE;
        }

        conn_info->db_idx = idx;
        set_status(ctx.get_resp(), "OK");
        return UPDATE_INFO_SUCCESS;
    }

    // If it is not a request to update information, make a copy of info
    // and continue processing the current request.
    info = *conn_info;
    return CONTINUE_PROCESS;
}

coke::Task<> redis_processor(coke::RedisServerContext ctx) {
    std::string cmd;
    std::vector<std::string> params;
    coke::RedisRequest &req = ctx.get_req();
    coke::RedisResponse &resp = ctx.get_resp();

    if (!req.get_command(cmd) || !req.get_params(params)) {
        set_error(resp, "ERROR Bad request");
        // Even without explicit use of ctx.reply, the message will be
        // returned to the client after the current coroutine returns
        co_return;
    }

    for (char &c : cmd)
        c = std::tolower(c);

    std::cout << "Process command " << cmd << std::endl;

    ConnInfo info;
    int ret = update_info(ctx, info, cmd, params);

    if (ret == UPDATE_INFO_FAILURE) {
        // Delay return to prevent attacks
        co_await coke::sleep(std::chrono::milliseconds(500));
        co_return;
    }

    if (ret == UPDATE_INFO_SUCCESS)
        co_return;

    // Successfully get user information, continue processing the request
    coke::RedisValue value;
    std::shared_mutex &mtx = db_mtx[info.db_idx];
    std::unordered_map<std::string, std::string> &db = database[info.db_idx];

    if (cmd == "get" && params.size() == 1) {
        std::shared_lock<std::shared_mutex> lg(mtx);
        auto it = db.find(params[0]);

        if (it == db.end())
            value.set_nil();
        else
            value.set_string(it->second);
    }
    else if (cmd == "set" && params.size() == 2) {
        std::unique_lock<std::shared_mutex> lg(mtx);
        db[params[0]] = params[1];
        value.set_status("OK");
    }
    else if (cmd == "del" && params.size() == 1) {
        std::unique_lock<std::shared_mutex> lg(mtx);
        auto it = db.find(params[0]);
        value.set_int(it == db.end() ? 0 : 1);

        if (it != db.end())
            db.erase(it);
    }
    else
        value.set_error("ERR unknown command");

    ctx.get_resp().set_result(value);

    // The reply will fail if the client disconnects before replying.
    // You can check the state and error code here.
    auto reply_result = co_await ctx.reply();

    std::cout << "User:" << info.username << " Db:" << info.db_idx
        << " Command:" << cmd
        << " State:" << reply_result.state << " Error:" << reply_result.error
        << std::endl;
}

// Usage: ./redis_server [port]

int main(int argc, char *argv[]) {
    int port = 8379;
    if (argc > 1)
        port = atoi(argv[1]);

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

void set_error(coke::RedisResponse &resp, const std::string &errmsg) {
    coke::RedisValue value;
    value.set_error(errmsg);
    resp.set_result(value);
}

void set_status(coke::RedisResponse &resp, const std::string &status) {
    coke::RedisValue value;
    value.set_status(status);
    resp.set_result(value);
}

int parse_db_index(const std::string &s) {
    try {
        int i = std::stoi(s);
        if (i < 0 || i >= MAX_DATABASE)
            return -1;
        return i;
    }
    catch (...) {
        return -1;
    }
}

bool check_user_pass(const std::vector<std::string> &params, std::string &username) {
    std::string password;

    if (params.size() == 1) {
        username.clear();
        password = params[0];
    }
    else if (params.size() == 2) {
        username = params[0];
        password = params[1];
    }
    else
        return false;

    auto it = user_pass.find(username);
    return it != user_pass.cend() && it->second == password;
}
