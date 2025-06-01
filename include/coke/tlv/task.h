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

#ifndef COKE_TLV_TASK_H
#define COKE_TLV_TASK_H

#include "coke/tlv/basic_types.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

class TlvClientTask : public WFComplexClientTask<TlvRequest, TlvResponse> {
public:
    TlvClientTask(int retry_max, tlv_callback_t &&callback);

    void set_client_info(const TlvClientInfo *info) { cli_info = info; }

    void set_close_connection() { close_connection = true; }

protected:
    virtual WFConnection *get_connection() const override;

    virtual CommMessageOut *message_out() override;

    virtual int keep_alive_timeout() override;

    virtual int first_timeout() override
    {
        return (is_user_req ? this->watch_timeo : 0);
    }

    virtual bool init_success() override;

    virtual bool finish_once() override;

protected:
    bool is_user_req;
    bool auth_failed;
    bool close_connection;

    const TlvClientInfo *cli_info;
};

} // namespace coke

#endif // COKE_TLV_TASK_H
