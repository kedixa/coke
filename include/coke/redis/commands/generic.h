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

#ifndef COKE_REDIS_COMMANDS_GENERIC_H
#define COKE_REDIS_COMMANDS_GENERIC_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

struct RedisRestoreOpt {
    bool replace{false};
    bool absttl{false};
    int64_t idle_seconds{-1};
    int64_t freq{-1};

    void append_to(StrHolderVec &vec) const
    {
        if (replace)
            vec.push_back("REPLACE"_sv);

        if (absttl)
            vec.push_back("ABSTTL"_sv);

        if (idle_seconds >= 0) {
            vec.push_back("IDLETIME"_sv);
            vec.push_back(std::to_string(idle_seconds));
        }

        if (freq >= 0) {
            vec.push_back("FREQ"_sv);
            vec.push_back(std::to_string(freq));
        }
    }

    std::size_t size() const
    {
        return (replace ? 1 : 0) + (absttl ? 1 : 0) +
               (idle_seconds >= 0 ? 2 : 0) + (freq >= 0 ? 2 : 0);
    }
};

struct RedisScanOpt {
    std::string match;
    uint64_t count{0};
    std::string type;

    void append_to(StrHolderVec &vec) const
    {
        if (!match.empty()) {
            vec.push_back("MATCH"_sv);
            vec.push_back(match);
        }

        if (count > 0) {
            vec.push_back("COUNT"_sv);
            vec.push_back(std::to_string(count));
        }

        if (!type.empty()) {
            vec.push_back("TYPE"_sv);
            vec.push_back(type);
        }
    }

    std::size_t size() const
    {
        return (match.empty() ? 0 : 2) + (count > 0 ? 2 : 0) +
               (type.empty() ? 0 : 2);
    }
};

template<typename Client>
struct RedisGenericCommands {
    /**
     * @brief Copy value at `src_key` to `dst_key`.
     * @return Integer, 0 if not copied, 1 if copied.
     *
     * Command: COPY source destination [DB destination-db] [REPLACE]
     *
     * Since Redis 6.2.0.
     */
    auto copy(StrHolder src_key, StrHolder dst_key, bool replace = false)
    {
        StrHolderVec v;
        std::size_t opt_size = (replace ? 1 : 0);

        v.reserve(3 + opt_size);
        v.push_back("COPY"_sv);
        v.push_back(std::move(src_key));
        v.push_back(std::move(dst_key));

        if (replace)
            v.push_back("REPLACE"_sv);

        return run(std::move(v));
    }

    /**
     * @brief Copy value at `src_key` to `dst_key`.
     * @return Integer, 0 if not copied, 1 if copied.
     *
     * Command: COPY source destination [DB destination-db] [REPLACE]
     *
     * Since Redis 6.2.0.
     */
    auto copy(StrHolder src_key, StrHolder dst_key, int dest_db, bool replace)
    {
        StrHolderVec v;
        std::size_t opt_size = (replace ? 1 : 0);

        v.reserve(3 + opt_size);
        v.push_back("COPY"_sv);
        v.push_back(std::move(src_key));
        v.push_back(std::move(dst_key));

        v.push_back("DB"_sv);
        v.push_back(std::to_string(dest_db));

        if (replace)
            v.push_back("REPLACE"_sv);

        return run(std::move(v));
    }

    /**
     * @brief Delete `keys`.
     * @return Integer, the number of keys that were removed.
     *
     * Command: DEL key [key ...]
     */
    auto del(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("DEL"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v));
    }

    /**
     * @brief Delete `keys`.
     * @return Integer, the number of keys that were removed.
     *
     * Command: DEL key [key ...]
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto del(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("DEL"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        return run(std::move(v));
    }

    /**
     * @brief Serialize the value stored at `key` in a Redis-specific format.
     * @return Bulk string, the serialized result, or Null if `key` not exists.
     *
     * Command: DUMP key
     */
    auto dump(StrHolder key)
    {
        auto v = make_shv("DUMP"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Check if `keys` exists.
     * @return Integer, the number of keys that exists.
     *
     * Command: EXISTS key [key ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 3.0.3, accepts multiple key arguments.
     */
    auto exists(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("EXISTS"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Check if `keys` exists.
     * @return Integer, the number of keys that exists.
     *
     * Command: EXISTS key [key ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 3.0.3, accepts multiple key arguments.
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto exists(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("EXISTS"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Set a timeout on `key`.
     * @return Integer, 0 if timeout was not set, 1 if timeout was set.
     *
     * Command: EXPIRE key seconds [NX | XX | GT | LT]
     *
     * Since Redis 1.0.0.
     * Since Redis 7.0.0, add NX, XX, GT, LT options.
     */
    auto expire(StrHolder key, int64_t seconds,
                const RedisExpireOpt &opt = RedisExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("EXPIRE"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(seconds));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Like expire but use unix timestamp seconds.
     *
     * Command: EXPIRE key unix-time-seconds [NX | XX | GT | LT]
     *
     * Since Redis 1.2.0.
     * Since Redis 7.0.0, add NX, XX, GT, LT options.
     */
    auto expireat(StrHolder key, int64_t timestamp,
                  const RedisExpireOpt &opt = RedisExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("EXPIREAT"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(timestamp));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Get the absolute unix timestamp which the `key` will expire.
     * @return the expiration Unix timestamp in seconds,
     *         -1 if no expiration,
     *         -2 if key not exists.
     *
     * Command: EXPIRETIME key
     *
     * Since Redis 7.0.0.
     */
    auto expiretime(StrHolder key)
    {
        auto v = make_shv("EXPIRETIME"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Move `key` to another `db`.
     * @return Integer, 1 if moved, 0 if not exists.
     *
     * Command: MOVE key db
     */
    auto move(StrHolder key, int32_t db)
    {
        auto v = make_shv("MOVE"_sv, std::move(key), std::to_string(db));
        return run(std::move(v));
    }

    /**
     * @brief Get the internal encoding for the Redis object stored at `key`.
     * @return Bulk string, the encoding of `key`, or Null if not exists.
     *
     * Command: OBJECT ENCODING key
     */
    auto object_encoding(StrHolder key)
    {
        auto v = make_shv("OBJECT"_sv, "ENCODING"_sv, std::move(key));
        return run(std::move(v), {.slot = -2, .flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the logarithmic access frequency counter of `key`.
     * @brief Integer, the value, or Null if not exists.
     *
     * Command: OBJECT FREQ key
     *
     * Since Redis 4.0.0.
     */
    auto object_freq(StrHolder key)
    {
        auto v = make_shv("OBJECT"_sv, "FREQ"_sv, std::move(key));
        return run(std::move(v), {.slot = -2, .flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the time in seconds since the last access to `key`.
     * @return Integer, the idle time, or Null if not exists.
     *
     * Command: OBJECT IDLETIME key
     */
    auto object_idletime(StrHolder key)
    {
        auto v = make_shv("OBJECT"_sv, "IDLETIME"_sv, std::move(key));
        return run(std::move(v), {.slot = -2, .flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the reference count of `key`.
     * @return Integer, the number of references, or Null if not exists.
     *
     * Command: OBJECT REFCOUNT key
     */
    auto object_refcount(StrHolder key)
    {
        auto v = make_shv("OBJECT"_sv, "REFCOUNT"_sv, std::move(key));
        return run(std::move(v), {.slot = -2, .flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Remove expire time on `key`.
     * @return Integer, 0 if `key` doesn't has timeout, 1 if timeout is removed.
     *
     * Command: PERSIST key
     */
    auto persist(StrHolder key)
    {
        auto v = make_shv("PERSIST"_sv, std::move(key));
        return run(std::move(v));
    }

    /**
     * @brief Like expire but use milliseconds.
     *
     * Command: PEXPIRE key milliseconds [NX | XX | GT | LT]
     *
     * Since Redis 2.6.0.
     * Since Redis 7.0.0, add NX, XX, GT, LT options.
     */
    auto pexpire(StrHolder key, int64_t milliseconds,
                 const RedisExpireOpt &opt = RedisExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("PEXPIRE"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(milliseconds));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Like expireat but use milliseconds.
     *
     * Command: PEXPIREAT key unix-time-milliseconds [NX | XX | GT | LT]
     *
     * Since Redis 2.6.0.
     * Since Redis 7.0.0, add NX, XX, GT, LT options.
     */
    auto pexpireat(StrHolder key, int64_t timestamp,
                   const RedisExpireOpt &opt = RedisExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("PEXPIREAT"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(timestamp));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Like expiretime but use milliseconds.
     *
     * Command: PEXPIRETIME key
     *
     * Since Redis 7.0.0.
     */
    auto pexpiretime(StrHolder key)
    {
        auto v = make_shv("PEXPIRETIME"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like ttl but return milliseconds.
     *
     * Command: PTTL key
     */
    auto pttl(StrHolder key)
    {
        auto v = make_shv("PTTL"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get a random key from current database.
     * @return Bulk string, a random key, or Null if database is empty.
     *
     * Command: RANDOMKEY
     */
    auto randomkey()
    {
        auto v = make_shv("RANDOMKEY"_sv);

        // Although randomkey is readonly command, don't use REDIS_FLAG_READONLY
        // to ensure always send to master node.
        return run(std::move(v), {.slot = REDIS_ANY_PRIMARY});
    }

    /**
     * @brief Renames `key` to `newname`.
     * @return Simple string OK.
     *
     * Command: RENAME key newkey
     *
     * Since Redis 1.0.0.
     * Since Redis 3.2.0, no longer returns error if `key` and `newname` are
     * same.
     */
    auto rename(StrHolder key, StrHolder newname)
    {
        auto v = make_shv("RENAME"_sv, std::move(key), std::move(newname));
        return run(std::move(v));
    }

    /**
     * @brief Renames `key` to `newname` if `newname` not exists.
     * @return Integer, 0 if `newname` exists, 1 if renamed.
     *
     * Command: RENAMENX key newkey
     *
     * Since Redis 1.0.0.
     * Since Redis 3.2.0, no longer returns error if `key` and `newname` are
     * same.
     */
    auto renamenx(StrHolder key, StrHolder newname)
    {
        auto v = make_shv("RENAMENX"_sv, std::move(key), std::move(newname));
        return run(std::move(v));
    }

    /**
     * @brief Create `key` with `serialized-value` (obtained via DUMP).
     * @return Simple string OK.
     *
     * Command: RESTORE key ttl serialized-value [REPLACE] [ABSTTL]
     *          [IDLETIME seconds] [FREQ frequency]
     *
     * Since Redis 2.6.0.
     * Since Redis 3.0.0, add REPLACE option.
     * Since Redis 5.0.0, add ABSTTL, IDLETIME, FREQ options.
     */
    auto restore(StrHolder key, int64_t ttl, StrHolder serialized_value,
                 const RedisRestoreOpt &opt = RedisRestoreOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(4 + opt_size);
        v.push_back("RESTORE"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(ttl));
        v.push_back(std::move(serialized_value));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Incrementally iterate over a collection of elements.
     * @attention Cluster client not support scan now.
     *
     * Command: SCAN cursor [MATCH pattern] [COUNT count] [TYPE type]
     *
     * Since Redis 2.8.0.
     * Since Redis 6.0.0, added TYPE option.
     */
    auto scan(StrHolder cursor, const RedisScanOpt &opt = RedisScanOpt())
        requires (!requires { typename Client::RedisClusterClientTag; })
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(2 + opt_size);
        v.push_back("SCAN"_sv);
        v.push_back(std::move(cursor));

        if (opt_size)
            opt.append_to(v);

        // Althrough scan is readonly command, don't use REDIS_FLAG_READONLY
        // to ensure always send to master node.
        return run(std::move(v));
    }

    /**
     * @brief Incrementally iterate over a collection of elements.
     * @attention Cluster client not support scan now.
     *
     * Command: SCAN cursor [MATCH pattern] [COUNT count] [TYPE type]
     *
     * Since Redis 2.8.0.
     * Since Redis 6.0.0, added TYPE option.
     */
    auto scan(StrHolder cursor, std::string pattern, uint64_t count,
              std::string type)
        requires (!requires { typename Client::RedisClusterClientTag; })
    {
        RedisScanOpt opt = {
            .match = pattern,
            .count = count,
            .type = type,
        };

        return scan(std::move(cursor), opt);
    }

    /**
     * @brief Alters the last access time of `keys`.
     * @return Integer, the number of keys that were touched.
     *
     * Command: TOUCH key [key ...]
     *
     * Since Redis 3.2.1.
     */
    auto touch(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("TOUCH"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        // Not use REDIS_FLAG_READONLY to ensure always send to master node.
        return run(std::move(v));
    }

    /**
     * @brief Alters the last access time of `keys`.
     * @return Integer, the number of keys that were touched.
     *
     * Command: TOUCH key [key ...]
     *
     * Since Redis 3.2.1.
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto touch(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("TOUCH"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        // Not use REDIS_FLAG_READONLY to ensure always send to master node.
        return run(std::move(v));
    }

    /**
     * @brief Get the time to live of `key`.
     * @return Integer, the time to live in seconds,
     *         -1 if no timeout, -2 if `key` not exists.
     *
     * Command: TTL key
     *
     * Since Redis 1.0.0.
     * Since Redis 2.8.0, return -2 if `key` not exists.
     */
    auto ttl(StrHolder key)
    {
        auto v = make_shv("TTL"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the type of value stored at `key`.
     * @return Simple string, the type of `key`, or "none" if not exists.
     *
     * Command: TYPE key
     */
    auto type(StrHolder key)
    {
        auto v = make_shv("TYPE"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Unlink `keys` from the database.
     * @return Integer, the number of keys that were unlinked.
     *
     * Command: UNLINK key [key ...]
     *
     * Since Redis 4.0.0.
     */
    auto unlink(RedisKeys keys)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 1);
        v.push_back("UNLINK"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v));
    }

    /**
     * @brief Unlink `keys` from the database.
     * @return Integer, the number of keys that were unlinked.
     *
     * Command: UNLINK key [key ...]
     *
     * Since Redis 4.0.0.
     */
    template<typename... Strs>
        requires (std::constructible_from<StrHolder, Strs &&> && ...)
    auto unlink(StrHolder key, Strs &&...keys)
    {
        StrHolderVec v;

        v.reserve(sizeof...(Strs) + 2);
        v.push_back("UNLINK"_sv);
        v.push_back(std::move(key));
        (v.push_back(std::forward<Strs>(keys)), ...);

        return run(std::move(v));
    }

    /**
     * @brief Get the current server time.
     * @return Array, the current server time in seconds and microseconds.
     *
     * Command: TIME
     */
    auto time()
    {
        RedisExecuteOption opt = {
            .slot = REDIS_ANY_PRIMARY,
            .flags = REDIS_FLAG_READONLY,
        };

        return run(make_shv("TIME"_sv), opt);
    }

    /**
     * @brief Return the `message` back.
     * @return Bulk string, the `message`.
     */
    auto echo(StrHolder message)
    {
        RedisExecuteOption opt = {
            .slot = REDIS_ANY_PRIMARY,
            .flags = REDIS_FLAG_READONLY,
        };

        return run(make_shv("ECHO"_sv, std::move(message)), opt);
    }

    /**
     * @brief Ping the server.
     * @return Simple string "PONG", or `message` if it is provided.
     *
     * Command: PING [message]
     */
    auto ping()
    {
        RedisExecuteOption opt = {
            .slot = REDIS_ANY_PRIMARY,
            .flags = REDIS_FLAG_READONLY,
        };

        return run(make_shv("PING"_sv), opt);
    }

    /**
     * @brief Ping the server.
     * @return Simple string "PONG", or `message` if it is provided.
     *
     * Command: PING [message]
     */
    auto ping(StrHolder message)
    {
        RedisExecuteOption opt = {
            .slot = REDIS_ANY_PRIMARY,
            .flags = REDIS_FLAG_READONLY,
        };

        return run(make_shv("PING"_sv, std::move(message)), opt);
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

#endif // COKE_REDIS_COMMANDS_GENERIC_H
