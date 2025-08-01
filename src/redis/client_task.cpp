/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
 */

#include "coke/redis/client_task.h"

namespace coke {

// clang-format off
enum : int {
    REDIS_CONN_AUTH      = 0,
    REDIS_CONN_SETNAME   = 1,
    REDIS_CONN_SELECT    = 2,
    REDIS_CONN_READONLY  = 3,
    REDIS_CONN_TRACKING  = 4,
    REDIS_CONN_LIBNAME   = 5,
    REDIS_CONN_LIBVER    = 6,
    REDIS_CONN_NOEVICT   = 7,
    REDIS_CONN_NOTOUCH   = 8,
    REDIS_USER_FIRST_REQ = 9,
    REDIS_USER_OTHER_REQ = 10,
};
// clang-format on

constexpr static int REDIS_KEEPALIVE_DEFAULT = 60 * 1000;

struct RedisConnInfo : public WFConnection {
    int next_stage{REDIS_CONN_AUTH};
    std::vector<int> current_stages;
    ClientConnInfo conn_info;
};

static void redis_conn_info_deleter(void *ptr)
{
    delete (RedisConnInfo *)ptr;
}

static int stage_error(int stage)
{
    switch (stage) {
    case REDIS_CONN_AUTH:
        return REDIS_ERR_AUTH;
    case REDIS_CONN_SETNAME:
        return REDIS_ERR_SETNAME;
    case REDIS_CONN_SELECT:
        return REDIS_ERR_SELECT;
    case REDIS_CONN_READONLY:
        return REDIS_ERR_READONLY;
    case REDIS_CONN_TRACKING:
        return REDIS_ERR_TRACKING;
    case REDIS_CONN_LIBNAME:
        return REDIS_ERR_LIBNAME;
    case REDIS_CONN_LIBVER:
        return REDIS_ERR_LIBVER;
    case REDIS_CONN_NOEVICT:
        return REDIS_ERR_NOEVICT;
    case REDIS_CONN_NOTOUCH:
        return REDIS_ERR_NOTOUCH;
    }

    return -1;
}

static StrHolderVec get_auth_command(const RedisClientInfo *info)
{
    StrHolderVec command;

    if (info->protover == 3) {
        command.reserve(7);
        command.emplace_back("HELLO"_sv);
        command.emplace_back("3"_sv);
        command.emplace_back("AUTH"_sv);

        if (info->username.empty())
            command.emplace_back("default"_sv);
        else
            command.emplace_back(info->username);

        command.emplace_back(info->password);

        if (!info->client_name.empty()) {
            command.emplace_back("SETNAME"_sv);
            command.emplace_back(info->client_name);
        }
    }
    else {
        command.reserve(3);
        command.emplace_back("AUTH"_sv);

        if (!info->username.empty())
            command.emplace_back(info->username);

        command.emplace_back(info->password);
    }

    return command;
}

static StrHolderVec get_tracking_command(const RedisClientInfo *info)
{
    StrHolderVec command;

    command.reserve(9 + 2 * info->tracking_prefixes.size());
    command.emplace_back("CLIENT"_sv);
    command.emplace_back("TRACKING"_sv);
    command.emplace_back("ON"_sv);

    if (!info->redirect_client_id.empty()) {
        command.emplace_back("REDIRECT"_sv);
        command.emplace_back(info->redirect_client_id);
    }

    for (const auto &prefix : info->tracking_prefixes) {
        command.emplace_back("PREFIX"_sv);
        command.emplace_back(prefix);
    }

    if (info->tracking_bcast)
        command.emplace_back("BCAST"_sv);

    if (info->tracking_optin)
        command.emplace_back("OPTIN"_sv);

    if (info->tracking_optout)
        command.emplace_back("OPTOUT"_sv);

    if (info->tracking_noloop)
        command.emplace_back("NOLOOP"_sv);

    return command;
}

RedisClientTask::RedisClientTask(int retry_max, redis_callback_t &&callback)
    : WFComplexClientTask(retry_max, std::move(callback))
{
    is_user_req = true;
    handshake_err = 0;
    close_connection = false;
    cli_info = nullptr;
}

WFConnection *RedisClientTask::get_connection() const
{
    WFConnection *conn = WFComplexClientTask::get_connection();

    if (conn) {
        void *ctx = conn->get_context();
        if (ctx)
            return (RedisConnInfo *)ctx;
    }

    return conn;
}

CommMessageOut *RedisClientTask::message_out()
{
    auto *conn = WFComplexClientTask::get_connection();
    auto *redis_conn = (RedisConnInfo *)conn->get_context();

    if (!redis_conn) {
        redis_conn = new RedisConnInfo();
        redis_conn->conn_info = cli_info->conn_info;
        conn->set_context(redis_conn, redis_conn_info_deleter);
    }

    if (close_connection) {
        this->disable_retry();
        errno = ENOTCONN;
        return nullptr;
    }

    redis_conn->current_stages.clear();
    switch (redis_conn->next_stage) {
    default: {
        RedisRequest *handshake_req;

        if (cli_info->pipe_handshake)
            handshake_req = handshake_with_pipe(redis_conn);
        else
            handshake_req = handshake_without_pipe(redis_conn);

        if (handshake_req) {
            std::size_t req_size = handshake_req->commands_size();
            get_resp()->prepare_pipeline(req_size);

            is_user_req = false;
            return handshake_req;
        }
    }

    case REDIS_USER_FIRST_REQ:
        if (this->is_fixed_conn()) {
            auto *route_target = (RouteManager::RouteTarget *)(this->target);

            if (route_target->state) {
                errno = ECONNRESET;
                return nullptr;
            }

            route_target->state = 1;
        }

        redis_conn->next_stage = REDIS_USER_OTHER_REQ;
        break;

    case REDIS_USER_OTHER_REQ:
        break;
    }

    RedisRequest *req = get_req();
    if (req->use_pipeline())
        get_resp()->prepare_pipeline(req->commands_size());

    return WFComplexClientTask::message_out();
}

int RedisClientTask::keep_alive_timeout()
{
    if (is_user_req)
        return this->keep_alive_timeo;

    auto *conn = WFComplexClientTask::get_connection();
    auto *redis_conn = (RedisConnInfo *)conn->get_context();

    // Handshake request is always array
    RedisResponse *resp = get_resp();
    RedisArray &array = resp->get_value().get_array();
    const std::vector<int> &stages = redis_conn->current_stages;

    for (std::size_t i = 0; i < stages.size(); i++) {
        if (array[i].is_error()) {
            RedisValue tmp = std::move(array[i]);
            resp->set_value(std::move(tmp));
            handshake_err = stage_error(stages[i]);
            return 0;
        }
    }

    handshake_err = 0;
    return REDIS_KEEPALIVE_DEFAULT;
}

bool RedisClientTask::init_success()
{
    if (!cli_info || !cli_info->conn_info.valid()) {
        this->state = WFT_STATE_TASK_ERROR;
        this->error = REDIS_ERR_NO_INFO;
        return false;
    }

    auto &conn_info = cli_info->conn_info;
    WFComplexClientTask::set_info(conn_info.get_short_info());

    if (conn_info.get_conn_id() != GENERIC_CLIENT_CONN_ID) {
        this->set_fixed_addr(true);
        this->set_fixed_conn(true);
    }

    return true;
}

bool RedisClientTask::finish_once()
{
    if (!is_user_req) {
        is_user_req = true;
        delete this->get_message_out();

        if (this->state == WFT_STATE_SUCCESS) {
            if (handshake_err == 0)
                get_resp()->reset();
            else {
                this->disable_retry();
                this->state = WFT_STATE_TASK_ERROR;
                this->error = handshake_err;
            }
        }

        return false;
    }

    if (this->is_fixed_conn()) {
        if (this->state != WFT_STATE_SUCCESS || this->keep_alive_timeo == 0) {
            if (this->target)
                ((RouteManager::RouteTarget *)(this->target))->state = 0;
        }
    }

    return true;
}

RedisRequest *RedisClientTask::handshake_without_pipe(void *conn)
{
    RedisConnInfo *redis_conn = (RedisConnInfo *)conn;
    StrHolderVec command;

    switch (redis_conn->next_stage) {
    case REDIS_CONN_AUTH:
        if (!cli_info->password.empty()) {
            command = get_auth_command(cli_info);

            redis_conn->current_stages.push_back(REDIS_CONN_AUTH);
            redis_conn->next_stage = REDIS_CONN_SETNAME;
            break;
        }

    case REDIS_CONN_SETNAME:
        if (cli_info->protover != 3 && !cli_info->client_name.empty()) {
            command = make_shv("CLIENT"_sv, "SETNAME"_sv,
                               cli_info->client_name);

            redis_conn->current_stages.push_back(REDIS_CONN_SETNAME);
            redis_conn->next_stage = REDIS_CONN_SELECT;
            break;
        }

    case REDIS_CONN_SELECT:
        if (cli_info->database != 0) {
            command = make_shv("SELECT"_sv, std::to_string(cli_info->database));

            redis_conn->current_stages.push_back(REDIS_CONN_SELECT);
            redis_conn->next_stage = REDIS_CONN_READONLY;
            break;
        }

    case REDIS_CONN_READONLY:
        if (cli_info->read_replica) {
            command = make_shv("READONLY"_sv);

            redis_conn->current_stages.push_back(REDIS_CONN_READONLY);
            redis_conn->next_stage = REDIS_CONN_TRACKING;
            break;
        }

    case REDIS_CONN_TRACKING:
        if (cli_info->enable_tracking) {
            command = get_tracking_command(cli_info);

            redis_conn->current_stages.push_back(REDIS_CONN_TRACKING);
            redis_conn->next_stage = REDIS_CONN_LIBNAME;
            break;
        }

    case REDIS_CONN_LIBNAME:
        if (!cli_info->lib_name.empty()) {
            command = make_shv("CLIENT"_sv, "SETINFO"_sv, "LIB-NAME"_sv,
                               cli_info->lib_name);

            redis_conn->current_stages.push_back(REDIS_CONN_LIBNAME);
            redis_conn->next_stage = REDIS_CONN_LIBVER;
            break;
        }

    case REDIS_CONN_LIBVER:
        if (!cli_info->lib_ver.empty()) {
            command = make_shv("CLIENT"_sv, "SETINFO"_sv, "LIB-VER"_sv,
                               cli_info->lib_ver);

            redis_conn->current_stages.push_back(REDIS_CONN_LIBVER);
            redis_conn->next_stage = REDIS_CONN_NOEVICT;
            break;
        }

    case REDIS_CONN_NOEVICT:
        if (cli_info->no_evict) {
            command = make_shv("CLIENT"_sv, "NO-EVICT"_sv, "ON"_sv);

            redis_conn->current_stages.push_back(REDIS_CONN_NOEVICT);
            redis_conn->next_stage = REDIS_CONN_NOTOUCH;
            break;
        }

    case REDIS_CONN_NOTOUCH:
        if (cli_info->no_touch) {
            command = make_shv("CLIENT"_sv, "NO-TOUCH", "ON"_sv);

            redis_conn->current_stages.push_back(REDIS_CONN_NOTOUCH);
            redis_conn->next_stage = REDIS_USER_FIRST_REQ;
            break;
        }
    }

    if (!command.empty()) {
        RedisRequest *req = new RedisRequest();
        req->add_command(std::move(command));
        handshake_err = -1;
        return req;
    }

    handshake_err = 0;
    return nullptr;
}

RedisRequest *RedisClientTask::handshake_with_pipe(void *conn)
{
    RedisConnInfo *redis_conn = (RedisConnInfo *)conn;
    std::vector<StrHolderVec> commands;
    StrHolderVec command;
    auto &stages = redis_conn->current_stages;

    if (!cli_info->password.empty()) {
        commands.push_back(get_auth_command(cli_info));
        stages.push_back(REDIS_CONN_AUTH);
    }

    if (cli_info->protover != 3 && !cli_info->client_name.empty()) {
        command = make_shv("CLIENT"_sv, "SETNAME"_sv, cli_info->client_name);
        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_SETNAME);
    }

    if (cli_info->database != 0) {
        command = make_shv("SELECT"_sv, std::to_string(cli_info->database));

        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_SELECT);
    }

    if (cli_info->read_replica) {
        command = make_shv("READONLY"_sv);
        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_READONLY);
    }

    if (cli_info->enable_tracking) {
        command = get_tracking_command(cli_info);
        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_TRACKING);
    }

    if (!cli_info->lib_name.empty()) {
        command = make_shv("CLIENT"_sv, "SETINFO"_sv, "LIB-NAME"_sv,
                           cli_info->lib_name);

        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_LIBNAME);
    }

    if (!cli_info->lib_ver.empty()) {
        command = make_shv("CLIENT"_sv, "SETINFO"_sv, "LIB-VER"_sv,
                           cli_info->lib_ver);

        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_LIBVER);
    }

    if (cli_info->no_evict) {
        command = make_shv("CLIENT"_sv, "NO-EVICT"_sv, "ON"_sv);
        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_NOEVICT);
    }

    if (cli_info->no_touch) {
        command = make_shv("CLIENT"_sv, "NO-TOUCH"_sv, "ON"_sv);
        commands.push_back(std::move(command));
        stages.push_back(REDIS_CONN_NOTOUCH);
    }

    redis_conn->next_stage = REDIS_USER_FIRST_REQ;

    if (!commands.empty()) {
        RedisRequest *req = new RedisRequest();

        req->reserve_commands(commands.size());
        for (StrHolderVec &c : commands)
            req->add_command(std::move(c));

        handshake_err = -1;
        return req;
    }

    handshake_err = 0;
    return nullptr;
}

} // namespace coke
