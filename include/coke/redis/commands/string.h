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

#ifndef COKE_REDIS_COMMANDS_STRING_H
#define COKE_REDIS_COMMANDS_STRING_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

using RedisGetexExpireOpt =
    RedisOptsChoice<RedisOptNone, RedisOptEx, RedisOptPx, RedisOptExat,
                    RedisOptPxat, RedisOptPersist>;

struct RedisLcsOpt {
    bool len{false};
    bool idx{false};
    bool with_match_len{false};
    int32_t min_match_len{-1};

    void append_to(StrHolderVec &vec) const
    {
        if (len)
            vec.push_back("LEN"_sv);

        if (idx)
            vec.push_back("IDX"_sv);

        if (min_match_len >= 0) {
            vec.push_back("MINMATCHLEN"_sv);
            vec.push_back(std::to_string(min_match_len));
        }

        if (with_match_len)
            vec.push_back("WITHMATCHLEN"_sv);
    }

    std::size_t size() const
    {
        return (len ? 1 : 0) + (idx ? 1 : 0) + (with_match_len ? 1 : 0) +
               (min_match_len >= 0 ? 2 : 0);
    }
};

struct RedisSetOpt {
    using ExistOpt = RedisOptsChoice<RedisOptNone, RedisOptNx, RedisOptXx>;

    bool get{false};
    ExistOpt exists;
    RedisSetExpireOpt expire;

    void append_to(StrHolderVec &vec) const
    {
        if (get)
            vec.push_back("GET"_sv);

        exists.append_to(vec);
        expire.append_to(vec);
    }

    std::size_t size() const
    {
        return (get ? 1 : 0) + exists.size() + expire.size();
    }
};

template<typename Client>
struct RedisStringCommands {
    /**
     * @brief Append `value` to the value at `key`.
     * @return The new length of value at `key`.
     *
     * Command: APPEND key value
     */
    auto append(StrHolder key, StrHolder value)
    {
        auto v = make_shv("APPEND"_sv, std::move(key), std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Decrements the number stored at `key` by one.
     * @return The value of `key` after decrement.
     *
     * Command: DECR key
     */
    auto decr(StrHolder key)
    {
        auto v = make_shv("DECR"_sv, std::move(key));
        return run(std::move(v));
    }

    /**
     * @brief Decrements the number stored at `key` by `value`.
     * @return The value of `key` after decrement.
     *
     * Command: DECRBY key value
     */
    auto decrby(StrHolder key, int64_t value)
    {
        auto v = make_shv("DECRBY"_sv, std::move(key), std::to_string(value));
        return run(std::move(v));
    }

    /**
     * @brief Get the value of `key`.
     * @return string, the value of `key`.
     * @return Null if `key` does not exist.
     *
     * Command: GET key
     */
    auto get(StrHolder key)
    {
        auto v = make_shv("GET"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the value of `key`.
     * @return string, the value of `key`.
     * @return Null if `key` does not exist or value is not string.
     *
     * Command: GETDEL key
     */
    auto getdel(StrHolder key)
    {
        auto v = make_shv("GETDEL"_sv, std::move(key));
        return run(std::move(v));
    }

    /**
     * @brief Get the value of `key` and optionally set its expiration.
     * @return Same as GET.
     *
     * Command: GETEX key [EX seconds | PX milliseconds
     *         | EXAT unix-time-seconds | PXAT unix-time-milliseconds | PERSIST]
     */
    auto getex(StrHolder key,
               const RedisGetexExpireOpt &opt = RedisGetexExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(2 + opt_size);
        v.push_back("GETEX"_sv);
        v.push_back(std::move(key));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Get the substring at `key` determined by `start` and `end`.
     * @return The substring.
     *
     * Command: GETRANGE key start end
     */
    auto getrange(StrHolder key, int64_t start, int64_t end)
    {
        auto v = make_shv("GETRANGE"_sv, std::move(key), std::to_string(start),
                          std::to_string(end));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Atomically sets `key` to `value` and returns the old value.
     * @return string, the previous value of `key`.
     * @return Null if `key` does not exist.
     *
     * Command: GETSET key value
     *
     * Deprecated since Redis 6.2.0, use SET with GET argument.
     */
    auto getset(StrHolder key, StrHolder value)
    {
        auto v = make_shv("GETSET"_sv, std::move(key), std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Increments the number stored at `key` by one.
     * @return The value of `key` after increment.
     *
     * Command: INCR key
     */
    auto incr(StrHolder key)
    {
        auto v = make_shv("INCR"_sv, std::move(key));
        return run(std::move(v));
    }

    /**
     * @brief Increments the number stored at `key` by `value`.
     * @return The value of `key` after increment.
     *
     * Command: INCRBY key value
     */
    auto incrby(StrHolder key, int64_t value)
    {
        auto v = make_shv("INCRBY"_sv, std::move(key), std::to_string(value));
        return run(std::move(v));
    }

    /**
     * @brief Increments the number stored at `key` by float `value`.
     * @return The value of `key` after increment.
     *
     * Command: INCRBYFLOAT key value
     */
    auto incrbyfloat(StrHolder key, double value)
    {
        auto v = make_shv("INCRBYFLOAT"_sv, std::move(key),
                          std::to_string(value));
        return run(std::move(v));
    }

    /**
     * @brief Find the longest common subsequence between `key1` and `key2`.
     *
     * Command: LCS key1 key2 [LEN] [IDX] [MINMATCHLEN min-match-len]
     *          [WITHMATCHLEN]
     *
     * Since Redis 7.0.0.
     */
    auto lcs(StrHolder key1, StrHolder key2,
             const RedisLcsOpt &opt = RedisLcsOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("LCS"_sv);
        v.push_back(std::move(key1));
        v.push_back(std::move(key2));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the value of all `keys`.
     * @return Array of values.
     *
     * Command: MGET key [key ...]
     */
    auto mget(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("MGET"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the value of all `keys`.
     * @return Array of values.
     *
     * Command: MGET key [key ...]
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto mget(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("MGET"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Sets the given keys to their respective values.
     * @return Simple string OK.
     *
     * Command: MSET key value [key value ...]
     */
    auto mset(RedisKeyValues kvs)
    {
        StrHolderVec v;

        v.reserve(kvs.size() * 2 + 1);
        v.push_back("MSET"_sv);

        for (auto &kv : kvs) {
            v.push_back(kv.key);
            v.push_back(kv.value);
        }

        return run(std::move(v));
    }

    /**
     * @brief Sets the given keys to their respective values.
     * @return Simple string OK.
     *
     * Command: MSET key value [key value ...]
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto mset(StrHolder key, StrHolder value, Strs &&...kvs)
    {
        static_assert(sizeof...(Strs) % 2 == 0, "kvs must be even");

        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 3);
        v.push_back("MSET"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(value));
        (v.push_back(std::forward<Strs>(kvs)), ...);

        return run(std::move(v));
    }

    /**
     * @brief Like MSET, but will not perform any operation if any key exists.
     * @return Integer 0 if no key was set.
     * @return Integer 1 if all keys were set.
     *
     * Command: MSETNX key value [key value ...]
     */
    auto msetnx(RedisKeyValues kvs)
    {
        StrHolderVec v;

        v.reserve(kvs.size() * 2 + 1);
        v.push_back("MSETNX"_sv);

        for (auto &kv : kvs) {
            v.push_back(kv.key);
            v.push_back(kv.value);
        }

        return run(std::move(v));
    }

    /**
     * @brief Like MSET, but will not perform any operation if any key exists.
     * @return Integer 0 if no key was set.
     * @return Integer 1 if all keys were set.
     *
     * Command: MSETNX key value [key value ...]
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto msetnx(StrHolder key, StrHolder value, Strs &&...kvs)
    {
        static_assert(sizeof...(Strs) % 2 == 0, "kvs must be even");

        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 3);
        v.push_back("MSETNX"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(value));
        (v.push_back(std::forward<Strs>(kvs)), ...);

        return run(std::move(v));
    }

    /**
     * @brief Like SETEX but use milliseconds as expire time.
     *
     * Command: PSETEX key milliseconds value
     *
     * Deprecated since Redis 2.6.12, use SET with PX argument.
     */
    auto psetex(StrHolder key, int64_t milliseconds, StrHolder value)
    {
        auto v = make_shv("PSETEX"_sv, std::move(key),
                          std::to_string(milliseconds), std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Set `value` to `key`.
     * @return Null, if with GET option, and `key` didn't exist before set.
     * @return Bulk string, if with GET option, the previous value of `key`.
     * @return Null, without GET option, aborted by one of NX/XX option.
     * @return Simple string OK, without GET option, the `key` was set.
     *
     * Command: SET key value [NX | XX] [GET] [EX seconds | PX milliseconds |
     *          EXAT unix-time-seconds | PXAT unix-time-milliseconds | KEEPTTL]
     *
     * Since Redis 2.6.12, added the EX, PX, NX and XX options.
     * Since Redis 6.0.0: added the KEEPTTL option.
     * Since Redis 6.2.0: added the GET, EXAT and PXAT option.
     * Since Redis 7.0.0: allowed the NX and GET options to be used together.
     */
    auto set(StrHolder key, StrHolder value,
             const RedisSetOpt &opt = RedisSetOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt.size());
        v.push_back("SET"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(value));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Set `value` to `key` with expire.
     *
     * Command: SETEX key seconds value
     *
     * Deprecated since Redis 2.6.12, use SET with EX argument.
     */
    auto setex(StrHolder key, int64_t seconds, StrHolder value)
    {
        auto v = make_shv("SETEX"_sv, std::move(key), std::to_string(seconds),
                          std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Set `value` to `key` if `key` not exist.
     *
     * Command: SETNX key value
     *
     * Deprecated since Redis 2.6.12, use SET with NX argument.
     */
    auto setnx(StrHolder key, StrHolder value)
    {
        auto v = make_shv("SETNX"_sv, std::move(key), std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Overwrite a range of value at `key` start at `offset`.
     * @return The length of string after overwrite.
     *
     * Command: SETRANGE key offset value
     */
    auto setrange(StrHolder key, int32_t offset, StrHolder value)
    {
        auto v = make_shv("SETRANGE"_sv, std::move(key), std::to_string(offset),
                          std::move(value));
        return run(std::move(v));
    }

    /**
     * @brief Get the length of value at `key`.
     * @return Length of the value at `key`, or 0 when `key` does not exist.
     *
     * Command: STRLEN key
     */
    auto strlen(StrHolder key)
    {
        auto v = make_shv("STRLEN"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get substring of the value at `key`.
     * @return The substring.
     *
     * Command: SUBSTR key start end
     *
     * Deprecated since Redis 2.0.0, use GETRANGE.
     */
    auto substr(StrHolder key, int32_t start, int32_t end)
    {
        auto v = make_shv("SUBSTR"_sv, std::move(key), std::to_string(start),
                          std::to_string(end));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
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

#endif // COKE_REDIS_COMMANDS_STRING_H
