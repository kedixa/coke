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

#ifndef COKE_REDIS_COMMANDS_PUBLISH_H
#define COKE_REDIS_COMMANDS_PUBLISH_H

#include "coke/redis/basic_types.h"

namespace coke {

template<typename Client>
struct RedisPublishCommands {
    /**
     * @brief Posts a message to the given channel.
     * @return Integer reply, the number of clients that the message was sent
     *         to.
     *
     * Command: PUBLISH channel message
     */
    auto publish(StrHolder channel, StrHolder message)
    {
        auto v = make_shv("PUBLISH"_sv, std::move(channel), std::move(message));
        return run(std::move(v), {.slot = REDIS_ANY_PRIMARY});
    }

    /**
     * @brief Lists the currently active channels.
     * @return Array reply, a list of active channels.
     *
     * Command: PUBSUB CHANNELS [pattern]
     */
    auto pubsub_channels(StrHolder pattern)
    {
        auto v = make_shv("PUBSUB"_sv, "CHANNELS"_sv, std::move(pattern));
        return run(std::move(v), {.slot = REDIS_ANY_PRIMARY});
    }

    /**
     * @brief The number of unique patterns that are subscribed to by clients.
     * @return Integer reply.
     *
     * Command: PUBSUB NUMPAT
     */
    auto pubsub_numpat()
    {
        auto v = make_shv("PUBSUB"_sv, "NUMPAT"_sv);
        return run(std::move(v), {.slot = REDIS_ANY_PRIMARY});
    }

    /**
     * @brief The number of subscribers (exclusive of clients subscribed to
     *        patterns) for the specified channels.
     * @return Array reply, a list of channels and number of subscribers.
     *
     * Command: PUBSUB NUMSUB channel [channel ...]
     */
    auto pubsub_numsub(StrHolderVec channels)
    {
        StrHolderVec v;

        v.reserve(2 + channels.size());
        v.push_back("PUBSUB"_sv);
        v.push_back("NUMSUB"_sv);

        for (auto &ch : channels)
            v.push_back(std::move(ch));

        return run(std::move(v), {.slot = REDIS_ANY_PRIMARY});
    }

    /**
     * @brief Posts a message to the given shard channel.
     * @return Integer reply, the number of clients that the message was sent
     *         to.
     *
     * Command: SPUBLISH shardchannel message
     * Since Redis 7.0.0.
     */
    auto spublish(StrHolder shardchannel, StrHolder message)
    {
        auto v = make_shv("SPUBLISH"_sv, std::move(shardchannel),
                          std::move(message));
        return run(std::move(v));
    }

private:
    Client &self() { return static_cast<Client &>(*this); }

    auto run(StrHolderVec &&args, RedisExecuteOption opt = {})
    {
        // If slot didn't set by command, reset to -1 which means use args[1]
        // as key and calculate slot from it.
        if (opt.slot == REDIS_AUTO_SLOT)
            opt.slot = -1;

        return self()._execute(std::move(args), opt);
    }
};

} // namespace coke

#endif // COKE_REDIS_COMMANDS_PUBLISH_H
