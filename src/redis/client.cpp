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

#include "coke/redis/client_impl.h"

#include "coke/redis/client_task.h"
#include "workflow/StringUtil.h"

namespace coke {

static void append_bool(std::string &info, const char *key, bool value)
{
    info.append(key).append("=");
    info.append(value ? "true" : "false").append("&");
}

static void append_int(std::string &info, const char *key, int value)
{
    info.append(key).append("=");
    info.append(std::to_string(value)).append("&");
}

static void append_string(std::string &info, const char *key,
                          const std::string &value)
{
    info.append(key).append("=");
    info.append(StringUtil::url_encode(value)).append("&");
}

void RedisClientImpl::init_client(bool unique_conn)
{
    cli_info.pipe_handshake = params.pipe_handshake;
    cli_info.read_replica = false;

    cli_info.protover = params.protover;
    cli_info.database = params.database;
    cli_info.username = params.username;
    cli_info.password = params.password;
    cli_info.client_name = params.client_name;
    cli_info.lib_name = params.lib_name;
    cli_info.lib_ver = params.lib_ver;

    cli_info.no_evict = params.no_evict;
    cli_info.no_touch = params.no_touch;

    cli_info.enable_tracking = false;
    cli_info.tracking_bcast = false;
    cli_info.tracking_optin = false;
    cli_info.tracking_optout = false;
    cli_info.tracking_noloop = false;
    cli_info.redirect_client_id.clear();
    cli_info.tracking_prefixes.clear();

    std::string info("coke:redis?");

    append_int(info, "protover", params.protover);
    append_int(info, "database", params.database);
    append_string(info, "username", params.username);
    append_string(info, "password", params.password);
    append_string(info, "client_name", params.client_name);
    append_string(info, "lib_name", params.lib_name);
    append_string(info, "lib_ver", params.lib_ver);
    append_bool(info, "no_evict", params.no_evict);
    append_bool(info, "no_touch", params.no_touch);

    // Tracking is not supported now.
    append_bool(info, "enable_tracking", false);

    info.pop_back();
    cli_info.conn_info = ClientConnInfo::create_instance(info, unique_conn);
}

Task<RedisResult> RedisClientImpl::_execute(StrHolderVec command,
                                            RedisExecuteOption opt)
{
    bool is_close_request = close_connection;
    int retry_max = params.retry_max;

    if (is_close_request) {
        retry_max = 0;
        close_connection = false;
    }

    TransportType type = (params.use_ssl ? TT_TCP_SSL : TT_TCP);
    auto *task = new RedisClientTask(retry_max, nullptr);
    task->set_client_info(&cli_info);
    task->set_ssl_ctx(params.ssl_ctx.get());

    if (params.host.empty()) {
        const sockaddr *addr = (const sockaddr *)&params.addr_storage;
        const std::string &info = cli_info.conn_info.get_short_info();
        task->init(type, addr, params.addr_len, info);
    }
    else {
        ParsedURI uri;
        uri.host = strdup(params.host.c_str());
        uri.port = strdup(params.port.c_str());

        if (uri.host && uri.port)
            uri.state = URI_STATE_SUCCESS;
        else {
            uri.state = URI_STATE_ERROR;
            uri.error = errno;
        }

        task->set_transport_type(type);
        task->init(std::move(uri));
    }

    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);

    if (opt.block_ms == 0)
        task->set_watch_timeout(params.default_watch_timeout);
    else if (opt.block_ms > 0)
        task->set_watch_timeout(opt.block_ms + params.watch_extra_timeout);

    if (is_close_request)
        task->set_close_connection();

    task->get_req()->set_command_nocopy(command);
    task->get_resp()->set_size_limit(params.response_size_limit);

    // The task is deleted before the next co_await.
    co_await RedisAwaiter(task);

    int state = task->get_state();
    int error = task->get_error();

    RedisResult result;
    if (state == WFT_STATE_SUCCESS) {
        RedisValue &value = task->get_resp()->get_value();
        result.set_value(std::move(value));
    }
    else if (is_close_request) {
        if (state == WFT_STATE_SYS_ERROR && error == ENOTCONN) {
            state = WFT_STATE_SUCCESS;
            error = 0;
        }
    }

    result.set_state(state);
    result.set_error(error);

    co_return result;
}

} // namespace coke
