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

#include "coke/basic_awaiter.h"
#include "coke/tlv/client.h"
#include "coke/tlv/task.h"
#include "workflow/StringUtil.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

class TlvAwaiter : public BasicAwaiter<void> {
public:
    TlvAwaiter(TlvTask *task)
    {
        task->set_callback([info = this->get_info()](void *) {
            auto *awaiter = info->get_awaiter<TlvAwaiter>();
            awaiter->done();
        });

        this->set_task(task);
    }
};

TlvClient::TlvClient(const TlvClientParams &params, bool unique_conn)
    : close_connection(false), params(params)
{
    cli_info.enable_auth       = params.enable_auth;
    cli_info.auth_type         = params.auth_type;
    cli_info.auth_success_type = params.auth_success_type;
    cli_info.auth_value        = params.auth_value;

    std::string info;

    info.assign("coke:tlv?");

    if (params.enable_auth) {
        info.append("enable_auth=true&");

        info.append("auth_type=")
            .append(std::to_string(params.auth_type))
            .append("&");

        info.append("auth_value=")
            .append(StringUtil::url_encode(params.auth_value))
            .append("&");

        info.append("auth_success_type=")
            .append(std::to_string(params.auth_success_type))
            .append("&");
    }
    else {
        info.append("enable_auth=false&");
    }

    info.pop_back();
    cli_info.conn_info = ClientConnInfo::create_instance(info, unique_conn);
}

Task<TlvResult> TlvClient::request(int32_t type, StrHolder value)
{
    bool is_close_request = close_connection;
    int retry_max         = params.retry_max;

    if (is_close_request) {
        retry_max        = 0;
        close_connection = false;
    }

    auto *task = new TlvClientTask(retry_max, nullptr);

    ParsedURI uri;
    uri.state = URI_STATE_SUCCESS;
    uri.host  = strdup(params.host.c_str());
    uri.port  = strdup(params.port.c_str());

    task->set_client_info(&cli_info);
    task->init(std::move(uri));

    task->set_transport_type(params.transport_type);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);
    task->set_watch_timeout(params.watch_timeout);

    if (is_close_request)
        task->set_close_connection();

    auto *req = task->get_req();

    req->set_type(type);

    if (value.holds_string()) {
        std::string &tmp = value.get_string();
        req->set_value(std::move(tmp));
    }
    else {
        std::string_view sv = value.get_view();
        req->set_value(std::string(sv));
    }

    task->get_resp()->set_size_limit(params.response_size_limit);

    co_await TlvAwaiter(task);

    int state = task->get_state();
    int error = task->get_error();

    TlvResult result;
    result.set_state(state);
    result.set_error(error);

    if (state == WFT_STATE_SUCCESS) {
        auto *resp = task->get_resp();

        result.set_type(resp->get_type());
        result.set_value(*(resp->get_value()));
    }
    else if (is_close_request) {
        if (state == WFT_STATE_SYS_ERROR && error == ENOTCONN) {
            result.set_state(WFT_STATE_SUCCESS);
            result.set_error(0);
        }
    }

    co_return result;
}

} // namespace coke
