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

#include "coke/tlv/task.h"
#include "workflow/WFTaskError.h"

namespace coke {

constexpr static int TLV_KEEPALIVE_DEFAULT = 60 * 1000;

enum : int {
    TLV_CONN_AUTH      = 0,
    TLV_USER_FIRST_REQ = 1,
    TLV_USER_OTHER_REQ = 2,
};

struct TlvConnInfo : public WFConnection {
    int next_stage{TLV_CONN_AUTH};
    ClientConnInfo conn_info;
};

static void tlv_conn_info_deleter(void *ptr)
{
    delete (TlvConnInfo *)ptr;
}

TlvClientTask::TlvClientTask(int retry_max, tlv_callback_t &&cb)
    : WFComplexClientTask(retry_max, std::move(cb))
{
    is_user_req      = false;
    auth_failed      = false;
    close_connection = false;
    cli_info         = nullptr;
}

WFConnection *TlvClientTask::get_connection() const
{
    WFConnection *conn = WFComplexClientTask::get_connection();

    if (conn) {
        void *ctx = conn->get_context();

        if (ctx)
            return (TlvConnInfo *)ctx;
    }

    return conn;
}

CommMessageOut *TlvClientTask::message_out()
{
    auto *conn     = WFComplexClientTask::get_connection();
    auto *tlv_conn = (TlvConnInfo *)conn->get_context();

    if (!tlv_conn) {
        tlv_conn            = new TlvConnInfo();
        tlv_conn->conn_info = cli_info->conn_info;
        conn->set_context(tlv_conn, tlv_conn_info_deleter);
    }

    TlvRequest *req = nullptr;

    is_user_req = true;

    if (close_connection) {
        this->disable_retry();
        errno = ENOTCONN;
        return nullptr;
    }

    switch (tlv_conn->next_stage) {
    case TLV_CONN_AUTH:
        if (cli_info->enable_auth) {
            req = new TlvRequest();
            req->set_type(cli_info->auth_type);
            req->set_value(cli_info->auth_value);

            is_user_req = false;
            auth_failed = false;

            tlv_conn->next_stage = TLV_USER_FIRST_REQ;
            break;
        }
        [[fallthrough]];

    case TLV_USER_FIRST_REQ:
        if (this->is_fixed_conn()) {
            auto *route_target = (RouteManager::RouteTarget *)(this->target);

            if (route_target->state) {
                errno = ECONNRESET;
                return nullptr;
            }

            route_target->state = 1;
        }

        tlv_conn->next_stage = TLV_USER_OTHER_REQ;
        break;

    case TLV_USER_OTHER_REQ:
    default:
        break;
    }

    if (req)
        return req;

    return WFComplexClientTask::message_out();
}

int TlvClientTask::keep_alive_timeout()
{
    if (is_user_req)
        return this->keep_alive_timeo;

    TlvResponse *resp = this->get_resp();

    if (resp->get_type() != cli_info->auth_success_type) {
        auth_failed = true;
        return 0;
    }

    return TLV_KEEPALIVE_DEFAULT;
}

bool TlvClientTask::init_success()
{
    if (!cli_info) {
        this->state = WFT_STATE_TASK_ERROR;
        this->error = TLV_ERR_CLI_INFO;
        return false;
    }

    WFComplexClientTask::set_transport_type(TT_TCP);
    WFComplexClientTask::set_info(cli_info->conn_info.get_short_info());

    auto conn_id = cli_info->conn_info.get_conn_id();
    if (conn_id != GENERIC_CLIENT_CONN_ID) {
        this->set_fixed_addr(true);
        this->set_fixed_conn(true);
    }

    return true;
}

bool TlvClientTask::finish_once()
{
    if (!is_user_req) {
        is_user_req = true;
        delete this->get_message_out();

        if (this->state == WFT_STATE_SUCCESS) {
            if (auth_failed) {
                this->disable_retry();
                this->state = WFT_STATE_TASK_ERROR;
                this->error = TLV_ERR_AUTH;
            }
            else {
                this->clear_resp();
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

} // namespace coke
