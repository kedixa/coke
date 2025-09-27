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

#ifndef COKE_REDIS_COMMANDS_HYPERLOGLOG_H
#define COKE_REDIS_COMMANDS_HYPERLOGLOG_H

#include "coke/redis/basic_types.h"

namespace coke {

template<typename Client>
struct RedisHyperloglogCommands {
    /**
     * @brief Adds all the `elements` to the HyperLogLog structure at `key`.
     * @return Integer reply, 1 if internal register was altered, else 0.
     *
     * Command: PFADD key [element [element ...]]
     */
    auto pfadd(StrHolder key, RedisElements elements)
    {
        StrHolderVec v;

        v.reserve(elements.size() + 2);
        v.push_back("PFADD"_sv);
        v.push_back(std::move(key));

        for (auto &ele : elements)
            v.push_back(std::move(ele));

        return run(std::move(v));
    }

    /**
     * @brief Get the approximated number of unique elements observed via PFADD.
     * @return Integer reply.
     *
     * Command: PFCOUNT key [key ...]
     */
    auto pfcount(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("PFCOUNT"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the approximated number of unique elements observed via PFADD.
     * @return Integer reply.
     *
     * Command: PFCOUNT key [key ...]
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto pfcount(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("PFCOUNT"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Merge multiple HyperLogLog values into a unique value.
     * @return Simple string reply OK.
     *
     * Command: PFMERGE destkey [sourcekey [sourcekey ...]]
     */
    auto pfmerge(StrHolder dest_key, StrHolderVec src_keys)
    {
        StrHolderVec v;

        v.reserve(src_keys.size() + 2);
        v.push_back("PFMERGE"_sv);
        v.push_back(std::move(dest_key));

        for (auto &key : src_keys)
            v.push_back(std::move(key));

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

#endif // COKE_REDIS_COMMANDS_HYPERLOGLOG_H
