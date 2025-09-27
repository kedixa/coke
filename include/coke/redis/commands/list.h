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

#ifndef COKE_REDIS_COMMANDS_LIST_H
#define COKE_REDIS_COMMANDS_LIST_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

using RedisListSideOpt = RedisOptsChoice<RedisOptLeft, RedisOptRight>;
using RedisListPosOpt = RedisOptsChoice<RedisOptBefore, RedisOptAfter>;

struct RedisLposOpt {
    int32_t rank{0};
    int32_t count{-1};
    int32_t maxlen{-1};

    void append_to(StrHolderVec &vec) const
    {
        if (rank != 0) {
            vec.emplace_back("RANK"_sv);
            vec.emplace_back(std::to_string(rank));
        }

        if (count >= 0) {
            vec.emplace_back("COUNT"_sv);
            vec.emplace_back(std::to_string(count));
        }

        if (maxlen >= 0) {
            vec.emplace_back("MAXLEN"_sv);
            vec.emplace_back(std::to_string(maxlen));
        }
    }

    std::size_t size() const
    {
        return (rank != 0 ? 2 : 0) + (count >= 0 ? 2 : 0) +
               (maxlen >= 0 ? 2 : 0);
    }
};

template<typename Client>
struct RedisListCommands {
    /**
     * @brief Like lmove but block.
     * @return Bulk string, the popped element, or Null if timeout.
     *
     * Command: BLMOVE source destination <LEFT | RIGHT> <LEFT | RIGHT> timeout
     *
     * Since Redis 6.2.0.
     */
    auto blmove(StrHolder src_key, StrHolder dst_key,
                const RedisListSideOpt &src_side,
                const RedisListSideOpt &dst_side, double timeout_sec)
    {
        StrHolderVec v;

        v.reserve(6);
        v.push_back("BLMOVE"_sv);
        v.push_back(std::move(src_key));
        v.push_back(std::move(dst_key));

        src_side.append_to(v);
        dst_side.append_to(v);
        v.push_back(std::to_string(timeout_sec));

        int32_t block_ms = static_cast<int32_t>(timeout_sec * 1000);
        return run(std::move(v), {.block_ms = block_ms});
    }

    /**
     * @brief Like lmpop but block.
     *
     * Command: BLMPOP timeout numkeys key [key ...] <LEFT | RIGHT>
     *          [COUNT count]
     *
     * Since Redis 7.0.0.
     */
    auto blmpop(double timeout_sec, RedisKeys keys,
                const RedisListSideOpt &pop_side)
    {
        return blmpop(timeout_sec, std::move(keys), pop_side, -1);
    }

    /**
     * @brief Like lmpop but block.
     *
     * Command: BLMPOP timeout numkeys key [key ...] <LEFT | RIGHT>
     *          [COUNT count]
     *
     * Since Redis 7.0.0.
     */
    auto blmpop(double timeout_sec, RedisKeys keys,
                const RedisListSideOpt &pop_side, int32_t count)
    {
        StrHolderVec v;
        std::size_t opt_size = (count >= 0 ? 2 : 0);

        v.reserve(keys.size() + 4 + opt_size);
        v.push_back("BLMPOP"_sv);
        v.push_back(std::to_string(timeout_sec));
        v.push_back(std::to_string(keys.size()));

        for (auto &key : keys)
            v.push_back(std::move(key));

        pop_side.append_to(v);

        if (opt_size) {
            v.push_back("COUNT"_sv);
            v.push_back(std::to_string(count));
        }

        int32_t block_ms = static_cast<int32_t>(timeout_sec * 1000);
        return run(std::move(v), {.slot = -3, .block_ms = block_ms});
    }

    /**
     * @brief Lpop from first non empty list in `keys`, or wait for at most
     *        `timeout_sec`.
     * @return Array of two bulk string, the key and popped element, if success,
     *         Null if timeout.
     *
     * Command: BLPOP key [key ...] timeout
     *
     * Since Redis 2.0.0.
     * Since Redis 6.0.0, timeout use double instead of an integer.
     */
    auto blpop(RedisKeys keys, double timeout_sec)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 2);
        v.push_back("BLPOP"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        v.push_back(std::to_string(timeout_sec));

        int32_t block_ms = static_cast<int32_t>(timeout_sec * 1000);
        return run(std::move(v), {.block_ms = block_ms});
    }

    /**
     * @brief Rpop from first non empty list in `keys`, or wait for at most
     *        `timeout_sec`.
     * @return Array of two bulk string, the key and popped element, if success,
     *         Null if timeout.
     *
     * Command: BRPOP key [key ...] timeout
     *
     * Since Redis 2.0.0.
     * Since Redis 6.0.0, timeout use double instead of an integer.
     */
    auto brpop(RedisKeys keys, double timeout_sec)
    {
        StrHolderVec v;

        v.reserve(keys.size() + 2);
        v.push_back("BRPOP"_sv);

        for (auto &key : keys)
            v.push_back(std::move(key));

        v.push_back(std::to_string(timeout_sec));

        int32_t block_ms = static_cast<int32_t>(timeout_sec * 1000);
        return run(std::move(v), {.block_ms = block_ms});
    }

    /**
     * @brief Like rpoplpush but block.
     *
     * Command: BRPOPLPUSH source destination timeout
     *
     * Since Redis 2.2.0.
     * Since Redis 6.0.0, timeout use double instead of an integer.
     * Deprecated since Redis 6.2.0, use BLMOVE.
     */
    auto brpoplpush(StrHolder src_key, StrHolder dst_key, double timeout_sec)
    {
        StrHolderVec v;

        v.reserve(4);
        v.push_back("BRPOPLPUSH"_sv);
        v.push_back(std::move(src_key));
        v.push_back(std::move(dst_key));
        v.push_back(std::to_string(timeout_sec));

        int32_t block_ms = static_cast<uint32_t>(timeout_sec * 1000);
        return run(std::move(v), {.block_ms = block_ms});
    }

    /**
     * @brief Get the element at index `index` in the list.
     * @return Bulk string, the element, or Null if `index` out of range.
     *
     * Command: LINDEX key index
     */
    auto lindex(StrHolder key, int32_t index)
    {
        auto v = make_shv("LINDEX"_sv, std::move(key), std::to_string(index));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Insert `element` before or after `pivot` in the list.
     * @return Integer, the length of the list after the insert,
     *         0 if key not exists, -1 if pivot not exists.
     *
     * Command: LINSERT key <BEFORE | AFTER> pivot element
     */
    auto linsert(StrHolder key, const RedisListPosOpt &pos, StrHolder pivot,
                 StrHolder element)
    {
        StrHolderVec v;

        v.reserve(5);
        v.push_back("LINSERT"_sv);
        v.push_back(std::move(key));
        pos.append_to(v);
        v.push_back(std::move(pivot));
        v.push_back(std::move(element));

        return run(std::move(v));
    }

    /**
     * @brief Get the length of the list.
     * @return Integer, the length of the list.
     *
     * Command: LLEN key
     */
    auto llen(StrHolder key)
    {
        auto v = make_shv("LLEN"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Move an element from one list to another.
     * @return Bulk string, the popped element.
     *
     * Command: LMOVE source destination <LEFT | RIGHT> <LEFT | RIGHT>
     *
     * Since Redis 6.2.0.
     */
    auto lmove(StrHolder src_key, StrHolder dst_key,
               const RedisListSideOpt &src_side,
               const RedisListSideOpt &dst_side)
    {
        StrHolderVec v;

        v.reserve(4);
        v.push_back("LMOVE"_sv);
        v.push_back(std::move(src_key));
        v.push_back(std::move(dst_key));

        src_side.append_to(v);
        dst_side.append_to(v);

        return run(std::move(v));
    }

    /**
     * @brief Pops elements from first non empty list in `keys`.
     * @return Array of bulk strings, the popped elements.
     *         Null if no element could be popped.
     *
     * Command: LMPOP numkeys key [key ...] <LEFT | RIGHT> [COUNT count]
     *
     * Since Redis 7.0.0.
     */
    auto lmpop(RedisKeys keys, const RedisListSideOpt &pop_side)
    {
        return lmpop(std::move(keys), pop_side, -1);
    }

    /**
     * @brief Pops elements from first non empty list in `keys`.
     * @return Array of bulk strings, the popped elements.
     *         Null if no element could be popped.
     *
     * Command: LMPOP numkeys key [key ...] <LEFT | RIGHT> [COUNT count]
     *
     * Since Redis 7.0.0.
     */
    auto lmpop(RedisKeys keys, const RedisListSideOpt &pop_side, int32_t count)
    {
        StrHolderVec v;
        std::size_t opt_size = (count >= 0 ? 2 : 0);

        v.reserve(3 + keys.size() + opt_size);
        v.push_back("LMPOP"_sv);
        v.push_back(std::to_string(keys.size()));

        for (auto &key : keys)
            v.push_back(std::move(key));

        pop_side.append_to(v);

        if (opt_size) {
            v.push_back("COUNT"_sv);
            v.push_back(std::to_string(count));
        }

        return run(std::move(v), {.slot = -2});
    }

    /**
     * @brief Pop the first element of the list.
     * @return Bulk string, the popped element, or Null if key not exists,
     *         array of bulk strings if with count.
     *
     * Command: LPOP key [count]
     */
    auto lpop(StrHolder key) { return lpop(std::move(key), -1); }

    /**
     * @brief Pop the first element of the list.
     * @return Bulk string, the popped element, or Null if key not exists,
     *         array of bulk strings if with count.
     *
     * Command: LPOP key [count]
     */
    auto lpop(StrHolder key, int32_t count)
    {
        StrHolderVec v;
        std::size_t opt_size = (count >= 0 ? 1 : 0);

        v.reserve(2 + opt_size);
        v.push_back("LPOP"_sv);
        v.push_back(std::move(key));

        if (opt_size)
            v.push_back(std::to_string(count));

        return run(std::move(v));
    }

    /**
     * @brief Get the position of matching `element` in the list.
     * @return Integer, the position of the element,
     *         array of integer if with COUNT,
     *         Null if no matching element.
     *
     * Command: LPOS key element [RANK rank] [COUNT num-matches] [MAXLEN len]
     *
     * Since Redis 6.2.0.
     */
    auto lpos(StrHolder key, StrHolder element,
              const RedisLposOpt &opt = RedisLposOpt())
    {
        StrHolderVec v;
        std::size_t opt_size = opt.size();

        v.reserve(3 + opt_size);
        v.push_back("LPOS"_sv);
        v.push_back(std::move(key));
        v.push_back(std::move(element));

        if (opt_size)
            opt.append_to(v);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Prepend `elements` to the list.
     * @return Integer, the length of the list after the push.
     *
     * Command: LPUSH key element [element ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 2.4.0, supports multiple elements.
     */
    auto lpush(StrHolder key, RedisElements elements)
    {
        StrHolderVec v;

        v.reserve(2 + elements.size());
        v.push_back("LPUSH"_sv);
        v.push_back(std::move(key));

        for (auto &element : elements)
            v.push_back(std::move(element));

        return run(std::move(v));
    }

    /**
     * @brief Prepend `elements` to the list only if the list exists.
     * @return Integer, the length of the list after the push.
     *
     * Command: LPUSHX key element [element ...]
     *
     * Since Redis 2.2.0.
     * Since Redis 4.0.0, supports multiple elements.
     */
    auto lpushx(StrHolder key, RedisElements elements)
    {
        StrHolderVec v;

        v.reserve(2 + elements.size());
        v.push_back("LPUSHX"_sv);
        v.push_back(std::move(key));

        for (auto &element : elements)
            v.push_back(std::move(element));

        return run(std::move(v));
    }

    /**
     * @brief Get a range of elements from the list.
     * @return Array of bulk strings, the elements in the range.
     *
     * Command: LRANGE key start stop
     */
    auto lrange(StrHolder key, int32_t start, int32_t stop)
    {
        auto v = make_shv("LRANGE"_sv, std::move(key), std::to_string(start),
                          std::to_string(stop));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Remove the first `count` occurrences of `element` from the list.
     *        If `count` is 0, remove all occurrences, if `count` < 0, remove
     *        from the end of the list.
     * @return Integer, the number of removed elements.
     *
     * Command: LREM key count element
     */
    auto lrem(StrHolder key, int32_t count, StrHolder element)
    {
        auto v = make_shv("LREM"_sv, std::move(key), std::to_string(count),
                          std::move(element));
        return run(std::move(v));
    }

    /**
     * @brief Set the element at index `index` in the list to `element`.
     * @return Simple string OK.
     *
     * Command: LSET key index element
     */
    auto lset(StrHolder key, int32_t index, StrHolder element)
    {
        auto v = make_shv("LSET"_sv, std::move(key), std::to_string(index),
                          std::move(element));
        return run(std::move(v));
    }

    /**
     * @brief Trim the list and keep the elements in the range.
     * @return Simple string OK.
     *
     * Command: LTRIM key start stop
     */
    auto ltrim(StrHolder key, int32_t start, int32_t stop)
    {
        auto v = make_shv("LTRIM"_sv, std::move(key), std::to_string(start),
                          std::to_string(stop));
        return run(std::move(v));
    }

    /**
     * @brief Pop the last element of the list.
     * @return Bulk string, the popped element, or Null if key not exists,
     *         array of bulk strings if with count.
     *
     * Command: RPOP key [count]
     */
    auto rpop(StrHolder key) { return rpop(std::move(key), -1); }

    /**
     * @brief Pop the last element of the list.
     * @return Bulk string, the popped element, or Null if key not exists,
     *         array of bulk strings if with count.
     *
     * Command: RPOP key [count]
     */
    auto rpop(StrHolder key, int32_t count)
    {
        StrHolderVec v;
        std::size_t opt_size = (count >= 0 ? 1 : 0);

        v.reserve(2 + opt_size);
        v.push_back("RPOP"_sv);
        v.push_back(std::move(key));

        if (opt_size)
            v.push_back(std::to_string(count));

        return run(std::move(v));
    }

    /**
     * @brief Move the last element of `src_key` to the front of `dst_key`.
     * @return Bulk string, the popped element, Null if `src_key` not exists.
     *
     * Command: RPOPLPUSH src_key dst_key
     *
     * Since Redis 1.2.0.
     * Deprecated since Redis 6.2.0, use LMOVE instead.
     */
    auto rpoplpush(StrHolder src_key, StrHolder dst_key)
    {
        auto v = make_shv("RPOPLPUSH"_sv, std::move(src_key),
                          std::move(dst_key));
        return run(std::move(v));
    }

    /**
     * @brief Append `elements` to the list.
     * @return Integer, the length of the list after the push.
     *
     * Command: RPUSH key element [element ...]
     *
     * Since Redis 1.0.0.
     * Since Redis 2.4.0, supports multiple elements.
     */
    auto rpush(StrHolder key, RedisElements elements)
    {
        StrHolderVec v;

        v.reserve(2 + elements.size());
        v.push_back("RPUSH"_sv);
        v.push_back(std::move(key));

        for (auto &element : elements)
            v.push_back(std::move(element));

        return run(std::move(v));
    }

    /**
     * @brief Append `elements` to the list only if the list exists.
     * @return Integer, the length of the list after the push.
     *
     * Command: RPUSHX key element [element ...]
     *
     * Since Redis 2.2.0.
     * Since Redis 4.0.0, supports multiple elements.
     */
    auto rpushx(StrHolder key, RedisElements elements)
    {
        StrHolderVec v;

        v.reserve(2 + elements.size());
        v.push_back("RPUSHX"_sv);
        v.push_back(std::move(key));

        for (auto &element : elements)
            v.push_back(std::move(element));

        return run(std::move(v));
    }

private:
    Client &self() { return static_cast<Client &>(*this); }

    auto run(StrHolderVec &&args, RedisExecuteOption opt = {})
    {
        if (opt.slot == REDIS_AUTO_SLOT)
            opt.slot = -1;

        return self()._execute(std::move(args), opt);
    }
};

} // namespace coke

#endif // COKE_REDIS_COMMANDS_LIST_H
