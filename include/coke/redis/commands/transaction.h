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

#ifndef COKE_REDIS_COMMANDS_TRANSACTION_H
#define COKE_REDIS_COMMANDS_TRANSACTION_H

#include "coke/redis/basic_types.h"

namespace coke {

/**
 * @attention Transaction commands is for RedisConnectionClient only.
 */
template<typename Client>
struct RedisTransactionCommand {
    /**
     * @brief Flushes all previously queued commands in a transaction and
     *        restores the connection state to normal.
     * @return Simple string OK.
     *
     * Command: DISCARD
     */
    auto discard()
    {
        auto v = make_shv("DISCARD"_sv);
        return run(std::move(v));
    }

    /**
     * @brief Executes all previously queued commands in a transaction and
     *        restores the connection state to normal.
     * @return Array reply, each element being the reply to each of the commands
     *         in the atomic transaction.
     *         Null reply if the transaction was aborted.
     *
     * Command: EXEC
     */
    auto exec()
    {
        auto v = make_shv("EXEC"_sv);
        return run(std::move(v));
    }

    /**
     * @brief Marks the start of a transaction block.
     * @return Simple string OK.
     *
     * Command: MULTI
     */
    auto multi()
    {
        auto v = make_shv("MULTI"_sv);
        return run(std::move(v));
    }

    /**
     * @brief Flushes all the previously watched keys for a transaction.
     * @return Simple string OK.
     *
     * Command: UNWATCH
     */
    auto unwatch()
    {
        auto v = make_shv("UNWATCH"_sv);
        return run(std::move(v));
    }

    /**
     * @brief Marks the given keys to be watched for conditional execution of a
     *        transaction.
     * @return Simple string OK.
     *
     * Command: WATCH
     */
    auto watch(StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("WATCH"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v));
    }

private:
    Client &self() { return static_cast<Client &>(*this); }

    auto run(StrHolderVec &&args, RedisExecuteOption opt = {})
    {
        // Transaction not supported by cluster client.
        opt.slot = REDIS_AUTO_SLOT;
        return self()._execute(std::move(args), opt);
    }
};

} // namespace coke

#endif // COKE_REDIS_COMMANDS_TRANSACTION_H
