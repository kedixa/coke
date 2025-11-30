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

#ifndef COKE_REDIS_COMMANDS_HASH_H
#define COKE_REDIS_COMMANDS_HASH_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

using RedisHexpireOpt = RedisExpireOpt;

struct RedisHscanOpt {
    std::string match;
    uint64_t count{0};
    bool novalues{false};

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

        if (novalues)
            vec.push_back("NOVALUES"_sv);
    }

    std::size_t size() const
    {
        return (match.empty() ? 0 : 2) + (count > 0 ? 2 : 0) +
               (novalues ? 1 : 0);
    }
};

struct RedisHsetexOpt {
    using ExistsOpt = RedisOptsChoice<RedisOptNone, RedisOptFnx, RedisOptFxx>;

    ExistsOpt exists;
    RedisSetExpireOpt expire;

    void append_to(StrHolderVec &vec) const
    {
        exists.append_to(vec);
        expire.append_to(vec);
    }

    std::size_t size() const { return exists.size() + expire.size(); }
};

template<typename Client>
struct RedisHashCommands {
    /**
     * @brief Removes the specified fields from the hash stored at `key`.
     * @return Integer reply, the number of fields that were removed.
     *
     * Command: HDEL key field [field ...]
     *
     * Since Redis 2.0.0.
     * Since Redis 2.4.0, accepts multiple field arguments.
     */
    auto hdel(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(2 + fields.size());
        v.push_back("HDEL"_sv);
        v.push_back(std::move(key));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Returns if field exist in the hash at `key`.
     * @return Integer reply, 0 or 1.
     *
     * Command: HEXISTS key field
     */
    auto hexists(StrHolder key, StrHolder field)
    {
        auto v = make_shv("HEXISTS"_sv, std::move(key), std::move(field));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Set expiration on fields of hash.
     * @return Array of integer reply, for each field
     *         -2 if field not exists,
     *         0 if NX, XX, GT, LT condition has not been met,
     *         1 if expire was set,
     *         2 if seconds is 0.
     *
     * Command: HEXPIRE key seconds [NX | XX | GT | LT] FIELDS numfields field
     *          [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hexpire(StrHolder key, int64_t seconds, RedisFields fields,
                 const RedisHexpireOpt &opt = RedisHexpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(5 + fields.size() + opt_size);
        v.push_back("HEXPIRE"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(seconds));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Set expiration on fields of hash.
     *
     * Command: HEXPIREAT key unix-time-seconds [NX | XX | GT | LT]
     *          FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hexpireat(StrHolder key, int64_t timestamp, RedisFields fields,
                   const RedisHexpireOpt &opt = RedisHexpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(5 + fields.size() + opt_size);
        v.push_back("HEXPIREAT"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(timestamp));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Get expiration time of fields of hash.
     * @return Array of integer reply, for each field
     *         -2 if field not exists,
     *         -1 if field has no expiration,
     *         >=0 if field has expiration time.
     *
     * Command: HEXPIRETIME key FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hexpiretime(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HEXPIRETIME"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the value of field in the hash stored at `key`.
     * @return Bulk string reply, the value of field,
     *         or Null if `key` does not exist or field does not exist.
     *
     * Command: HGET key field
     */
    auto hget(StrHolder key, StrHolder field)
    {
        auto v = make_shv("HGET"_sv, std::move(key), std::move(field));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get all the fields and values in the hash stored at `key`.
     * @return RESP2 Array of fields and values.
     * @return RESP3 Map of fields and values.
     *
     * Command: HGETALL key
     */
    auto hgetall(StrHolder key)
    {
        auto v = make_shv("HGETALL"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get and delete the value of field in the hash stored at `key`.
     * @return Array reply, for each field
     *         bulk string reply, the value of field,
     *         or Null if field does not exist.
     *
     * Command: HGETDEL key FIELDS numfields field [field ...]
     *
     * Since Redis 8.0.0.
     */
    auto hgetdel(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HGETDEL"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Get fields and set expire time.
     * @return Array of bulk string reply.
     *
     * Command: HGETEX key [EX seconds | PX milliseconds
     *          | EXAT unix-time-seconds | PXAT unix-time-milliseconds
     *          | PERSIST] FIELDS numfields field [field ...]
     *
     * Since Redis 8.0.0.
     */
    auto hgetex(StrHolder key, RedisFields fields,
                const RedisSetExpireOpt &opt = RedisSetExpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(4 + opt_size);
        v.push_back("HGETEX"_sv);
        v.push_back(std::move(key));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Increment the number stored at field in the hash stored at `key`.
     * @return Integer reply, the value of field after increment.
     *
     * Command: HINCRBY key field increment
     */
    auto hincrby(StrHolder key, StrHolder field, int64_t increment)
    {
        auto v = make_shv("HINCRBY"_sv, std::move(key), std::move(field),
                          std::to_string(increment));
        return run(std::move(v));
    }

    /**
     * @brief Increment the number stored at field in the hash stored at `key`.
     * @return Bulk string reply, the value of field after increment.
     *
     * Command: HINCRBYFLOAT key field increment
     */
    auto hincrbyfloat(StrHolder key, StrHolder field, double increment)
    {
        auto v = make_shv("HINCRBYFLOAT"_sv, std::move(key), std::move(field),
                          std::to_string(increment));
        return run(std::move(v));
    }

    /**
     * @brief Get all the fields in the hash stored at `key`.
     * @return Array of bulk string reply, the fields of the hash.
     *
     * Command: HKEYS key
     */
    auto hkeys(StrHolder key)
    {
        auto v = make_shv("HKEYS"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the number of fields in the hash stored at `key`.
     * @return Integer reply, the number of fields in the hash.
     *
     * Command: HLEN key
     */
    auto hlen(StrHolder key)
    {
        auto v = make_shv("HLEN"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the values of fields in the hash stored at `key`.
     * @return Array of bulk string reply, the values of fields.
     *
     * Command: HMGET key field [field ...]
     */
    auto hmget(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(2 + fields.size());
        v.push_back("HMGET"_sv);
        v.push_back(std::move(key));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Set multiple fields in the hash stored at `key`.
     * @return Simple string OK.
     *
     * Command: HMSET key field element [field element ...]
     *
     * Since Redis 2.0.0.
     * Deprecated since Redis 4.0.0, use HSET instead.
     */
    auto hmset(StrHolder key, RedisFieldElements fes)
    {
        StrHolderVec v;

        v.reserve(2 + fes.size() * 2);
        v.push_back("HMSET"_sv);
        v.push_back(std::move(key));

        for (auto &fe : fes) {
            v.push_back(std::move(fe.field));
            v.push_back(std::move(fe.element));
        }

        return run(std::move(v));
    }

    /**
     * @brief Remove the expire time on fields of hash.
     * @return Array of integer reply, for each field
     *         -2 if field not exists,
     *         -1 if field has no expiration,
     *         1 if expire was removed.
     *
     * Command: HPERSIST key FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hpersist(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HPERSIST"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Like hexpire but use milliseconds as expire time.
     *
     * Command: HPEXPIRE key milliseconds [NX | XX | GT | LT]
     *          FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hpexpire(StrHolder key, int64_t milliseconds, RedisFields fields,
                  const RedisHexpireOpt &opt = RedisHexpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(5 + fields.size() + opt_size);
        v.push_back("HPEXPIRE"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(milliseconds));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Like hexpireat but use milliseconds timestamp.
     *
     * Command: HPEXPIREAT key unix-time-milliseconds [NX | XX | GT | LT]
     *          FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hpexipreat(StrHolder key, int64_t milli_timestamp, RedisFields fields,
                    const RedisHexpireOpt &opt = RedisHexpireOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(5 + fields.size() + opt_size);
        v.push_back("HPEXPIREAT"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(milli_timestamp));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v));
    }

    /**
     * @brief Like hexpiretime but use milliseconds.
     *
     * Command: HPEXPIRETIME key FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hpexpiretime(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HPEXPIRETIME"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Like httl but use milliseconds.
     *
     * Command: HPTTL key FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto hpttl(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HPTTL"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get a random field from the hash stored at `key`.
     * @return Null if key not exists,
     *         bulk string reply, the field,
     *         array of bulk string reply, the fields if `count` is specified,
     *         array of field and value if `with_values` is true.
     *
     * Since Redis 6.2.0.
     */
    auto hrandfield(StrHolder key)
    {
        auto v = make_shv("HRANDFIELD"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get a random field from the hash stored at `key`.
     * @return Null if key not exists,
     *         bulk string reply, the field,
     *         array of bulk string reply, the fields if `count` is specified,
     *         array of field and value if `with_values` is true.
     *
     * Since Redis 6.2.0.
     */
    auto hrandfield(StrHolder key, int32_t count, bool with_values = false)
    {
        StrHolderVec v;

        v.reserve(3 + (with_values ? 1 : 0));
        v.push_back("HRANDFIELD"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(count));

        if (with_values)
            v.push_back("WITHVALUES"_sv);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Scan the hash stored at `key`.
     * @return Array reply, the first element is the next cursor,
     *         the second element is an array of fields and values.
     *
     * Command: HSCAN key cursor [MATCH pattern] [COUNT count] [NOVALUES]
     *
     * Since Redis 2.8.0.
     * Since Redis 7.4.0, added the NOVALUES flag.
     */
    auto hscan(StrHolder key, StrHolder cursor,
               const RedisHscanOpt &opt = RedisHscanOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("HSCAN"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(cursor));

        if (opt_size)
            opt.append_to(v);

        // Although hscan is readonly command, don't use REDIS_FLAG_READONLY
        // to ensure always send to master node.
        return run(std::move(v));
    }

    /**
     * @brief Set the string value of field in the hash stored at `key`.
     * @return Integer reply, the number of fields that were added.
     *
     * Command: HSET key field element [field element ...]
     *
     * Since Redis 2.0.0.
     * Since Redis 4.0.0, accepts multiple field arguments.
     */
    auto hset(StrHolder key, RedisFieldElements fes)
    {
        StrHolderVec v;

        v.reserve(2 + fes.size() * 2);
        v.push_back("HSET"_sv);
        v.push_back(std::move(key));

        for (auto &fe : fes) {
            v.push_back(std::move(fe.field));
            v.push_back(std::move(fe.element));
        }

        return run(std::move(v));
    }

    /**
     * @brief Set the value of fields and optionally set expire time.
     * @return Integer reply, 0 if no field was set, 1 if all fields were set.
     *
     * Command: HSETEX key [FNX | FXX] [EX seconds | PX milliseconds
     *          | EXAT unix-time-seconds | PXAT unix-time-milliseconds
     *          | KEEPTTL] FIELDS numfields field value [field value ...]
     *
     * Since Redis 8.0.0.
     */
    auto hsetex(StrHolder key, RedisFieldElements fes,
                const RedisHsetexOpt &opt = RedisHsetexOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(4 + fes.size() * 2 + opt_size);
        v.push_back("HSETEX"_sv);
        v.push_back(std::move(key));

        if (opt_size > 0)
            opt.append_to(v);

        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fes.size()));

        for (auto &fe : fes) {
            v.push_back(std::move(fe.field));
            v.push_back(std::move(fe.element));
        }

        return run(std::move(v));
    }

    /**
     * @brief Set field only if field not exists.
     * @return Integer reply, 0 if field exists, else 1.
     *
     * Command: HSETNX key field value
     */
    auto hsetnx(StrHolder key, StrHolder field, StrHolder element)
    {
        auto v = make_shv("HSETNX"_sv, std::move(key), std::move(field),
                          std::move(element));
        return run(std::move(v));
    }

    /**
     * @brief Get value length of field in hash at `key`.
     * @return The length of value, or 0 if field not exists.
     *
     * Command: HSTRLEN key field
     *
     * Since Redis 3.2.0.
     */
    auto hstrlen(StrHolder key, StrHolder field)
    {
        auto v = make_shv("HSTRLEN"_sv, std::move(key), std::move(field));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the remaining ttl of fields.
     * @return Array of integer reply, for each field
     *         -2 if field not exists,
     *         -1 if field has no expiration,
     *         >=0 the ttl in seconds.
     *
     * Command: HTTL key FIELDS numfields field [field ...]
     *
     * Since Redis 7.4.0.
     */
    auto httl(StrHolder key, RedisFields fields)
    {
        StrHolderVec v;

        v.reserve(4 + fields.size());
        v.push_back("HTTL"_sv);
        v.push_back(std::move(key));
        v.push_back("FIELDS"_sv);
        v.push_back(std::to_string(fields.size()));

        for (auto &field : fields)
            v.push_back(std::move(field));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get all values in hash at `key`.
     * @return Array of bulk string reply.
     *
     * Command: HVALS key
     */
    auto hvals(StrHolder key)
    {
        auto v = make_shv("HVALS"_sv, std::move(key));
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

#endif // COKE_REDIS_COMMANDS_HASH_H
