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

#ifndef COKE_REDIS_COMMANDS_SET_H
#define COKE_REDIS_COMMANDS_SET_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

struct RedisSscanOpt {
    std::string match;
    uint64_t count{0};

    void append_to(StrHolderVec &vec) const
    {
        if (!match.empty()) {
            vec.emplace_back("MATCH"_sv);
            vec.emplace_back(match);
        }

        if (count > 0) {
            vec.emplace_back("COUNT"_sv);
            vec.emplace_back(std::to_string(count));
        }
    }

    std::size_t size() const
    {
        return (match.empty() ? 0 : 2) + (count > 0 ? 2 : 0);
    }
};

template<typename Client>
struct RedisSetCommands {
    /**
     * @brief Add `members` to the set at `key`.
     * @return Integer, the number of members that were added to the set.
     *
     * Command: SADD key member [member ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 2.4.0, supports multiple members.
     */
    auto sadd(StrHolder key, StrHolderVec members)
    {
        StrHolderVec v;

        v.reserve(members.size() + 2);
        v.push_back("SADD"_sv);
        v.push_back(std::move(key));

        for (auto &member : members)
            v.push_back(std::move(member));

        return run(std::move(v));
    }

    /**
     * @brief Get the number of members in the set at `key`.
     * @return Integer, the number of members in the set.
     *
     * Command: SCARD key
     */
    auto scard(StrHolder key)
    {
        auto v = make_shv("SCARD"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the difference between the first set and the other sets.
     * @return Array of bulk strings.
     *
     * Command: SDIFF key [key ...]
     */
    auto sdiff(StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("SDIFF"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like sdiff but store the result in `dest`.
     * @return Integer, the number of members in the resulting set.
     *
     * Command: SDIFFSTORE dest key [key ...]
     */
    auto sdiffstore(StrHolder dest, StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 2);
        v.push_back("SDIFFSTORE"_sv);
        v.push_back(std::move(dest));

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v));
    }

    /**
     * @brief Get the intersection of the sets at `keys`.
     * @return Array of bulk strings, the members in the intersection.
     *
     * Command: SINTER key [key ...]
     */
    auto sinter(StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("SINTER"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like sinter but return the cardinality of the result set.
     * @return Integer, the number of members in the intersection.
     *
     * When provided with the optional LIMIT argument (which defaults to 0 and
     * means unlimited), if the intersection cardinality reaches limit partway
     * through the computation, the algorithm will exit and yield limit as the
     * cardinality.
     *
     * Command: SINTERCARD key [key ...] [LIMIT limit]
     */
    auto sintercard(StrHolderVec keys, uint64_t limit = 0)
    {
        StrHolderVec v;
        std::size_t opt_size = (limit == 0 ? 0 : 2);

        v.reserve(keys.size() + opt_size + 2);
        v.push_back("SINTERCARD"_sv);
        v.push_back(std::to_string(keys.size()));

        for (auto &key : keys)
            v.push_back(std::move(key));

        if (limit > 0) {
            v.push_back("LIMIT"_sv);
            v.push_back(std::to_string(limit));
        }

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like sinter but store the result in `dest`.
     * @return Integer, the number of members in the resulting set.
     *
     * Command: SINTERSTORE dest key [key ...]
     */
    auto sinterstore(StrHolder dest, StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 2);
        v.push_back("SINTERSTORE"_sv);
        v.push_back(std::move(dest));

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v));
    }

    /**
     * @brief Check if `member` is a member of the set stored at `key`.
     * @return Integer reply, 0 or 1.
     *
     * Command: SISMEMBER key member
     */
    auto sismember(StrHolder key, StrHolder member)
    {
        auto v = make_shv("SISMEMBER"_sv, std::move(key), std::move(member));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get all the members in the set at `key`.
     * @return RESP2 Array of bulk strings, the members in the set.
     *         RESP3 Set of bulk strings, the members in the set.
     *
     * Command: SMEMBERS key
     */
    auto smembers(StrHolder key)
    {
        auto v = make_shv("SMEMBERS"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Check if `members` are members of the set stored at `key`.
     * @return Array of integer reply.
     *
     * Command: SMISMEMBER key member [member ...]
     *
     * Since Redis 6.2.0.
     */
    auto smismember(StrHolder key, StrHolderVec members)
    {
        StrHolderVec v;

        v.reserve(members.size() + 2);
        v.push_back("SMISMEMBER"_sv);
        v.push_back(std::move(key));

        for (auto &member : members)
            v.push_back(std::move(member));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Move `member` from the set at `src` to the set at `dest`.
     * @return Integer reply, 1 if moved, or 0 if `member` not exists in `src`.
     *
     * Command: SMOVE src dest member
     */
    auto smove(StrHolder src, StrHolder dest, StrHolder member)
    {
        auto v = make_shv("SMOVE"_sv, std::move(src), std::move(dest),
                          std::move(member));
        return run(std::move(v));
    }

    /**
     * @brief Pop a random member from the set at `key`.
     * @return Bulk string reply, the popped member,
     *         Null if `key` not exists,
     *         Array of bulk strings if `count` is specified.
     *
     * Command: SPOP key [count]
     *
     * Since Redis 1.0.0.
     * Since Redis 3.2.0, supports `count` to pop multiple members.
     */
    auto spop(StrHolder key, uint64_t count = 0)
    {
        StrHolderVec v;
        std::size_t opt_size = (count == 0 ? 0 : 1);

        v.reserve(opt_size + 2);
        v.push_back("SPOP"_sv);
        v.push_back(std::move(key));

        if (count > 0)
            v.push_back(std::to_string(count));

        return run(std::move(v));
    }

    /**
     * @brief Like spop but not remove the member from the set.
     *
     * If `count` < 0, same element may be returned multiple times.
     *
     * Command: SRANDMEMBER key [count]
     */
    auto srandmember(StrHolder key)
    {
        auto v = make_shv("SRANDMEMBER"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like spop but not remove the member from the set.
     *
     * If `count` < 0, same element may be returned multiple times.
     *
     * Command: SRANDMEMBER key [count]
     */
    auto srandmember(StrHolder key, int64_t count)
    {
        auto v = make_shv("SRANDMEMBER"_sv, std::move(key),
                          std::to_string(count));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Remove `members` from the set at `key`.
     * @return Integer reply, the number of members that were removed.
     *
     * Command: SREM key member [member ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 2.4.0, supports multiple members.
     */
    auto srem(StrHolder key, StrHolderVec members)
    {
        StrHolderVec v;

        v.reserve(members.size() + 2);
        v.push_back("SREM"_sv);
        v.push_back(std::move(key));

        for (auto &member : members)
            v.push_back(std::move(member));

        return run(std::move(v));
    }

    /**
     * @brief Scan the set at `key`.
     *
     * Command: SSCAN key cursor [MATCH pattern] [COUNT count]
     */
    auto sscan(StrHolder key, StrHolder cursor,
               const RedisSscanOpt &opt = RedisSscanOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(opt_size + 3);
        v.push_back("SSCAN"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(cursor));

        if (opt_size)
            opt.append_to(v);

        // Althrough sscan is readonly command, don't use REDIS_FLAG_READONLY
        // to ensure always send to master node.
        return run(std::move(v));
    }

    /**
     * @brief Scan the set at `key`.
     *
     * Command: SSCAN key cursor [MATCH pattern] [COUNT count]
     */
    auto sscan(StrHolder key, StrHolder cursor, uint64_t count)
    {
        return sscan(std::move(key), std::move(cursor),
                     RedisSscanOpt{.count = count});
    }

    /**
     * @brief Get the union of the sets at `keys`.
     * @return RESP2 Array of bulk strings, the members in the union.
     *         RESP3 Set of bulk strings, the members in the union.
     *
     * Command: SUNION key [key ...]
     */
    auto sunion(StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("SUNION"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like sunion but store the result in `dest`.
     * @return Integer, the number of members in the resulting set.
     *
     * Command: SUNIONSTORE dest key [key ...]
     */
    auto sunionstore(StrHolder dest, StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 2);
        v.push_back("SUNIONSTORE"_sv);
        v.push_back(std::move(dest));

        for (auto &key : keys)
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

#endif // COKE_REDIS_COMMANDS_SET_H
