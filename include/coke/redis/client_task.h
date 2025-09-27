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

#ifndef COKE_REDIS_CLIENT_TASK_H
#define COKE_REDIS_CLIENT_TASK_H

#include "coke/basic_awaiter.h"
#include "coke/redis/basic_types.h"
#include "coke/redis/message.h"
#include "workflow/WFTaskFactory.h"

namespace coke {

using RedisTask = WFNetworkTask<RedisRequest, RedisResponse>;
using redis_callback_t = std::function<void(RedisTask *)>;

class RedisClientTask
    : public WFComplexClientTask<RedisRequest, RedisResponse> {
public:
    RedisClientTask(int retry_max, redis_callback_t &&callback);

    void set_client_info(const RedisClientInfo *info) { cli_info = info; }

    void set_close_connection() { close_connection = true; }

protected:
    WFConnection *get_connection() const override;

    CommMessageOut *message_out() override;

    int keep_alive_timeout() override;

    int first_timeout() override
    {
        return (is_user_req ? this->watch_timeo : 0);
    }

    bool init_success() override;

    bool finish_once() override;

protected:
    RedisRequest *handshake_with_pipe(void *redis_conn);
    RedisRequest *handshake_without_pipe(void *redis_conn);

protected:
    bool is_user_req;
    int handshake_err;
    bool close_connection;

    const RedisClientInfo *cli_info;
};

class RedisAwaiter : public BasicAwaiter<void> {
public:
    RedisAwaiter(RedisClientTask *task)
    {
        task->set_callback([info = this->get_info()](void *) {
            RedisAwaiter *a = info->get_awaiter<RedisAwaiter>();
            a->done();
        });

        this->set_task(task);
    }
};

} // namespace coke

#endif // COKE_REDIS_CLIENT_TASK_H
