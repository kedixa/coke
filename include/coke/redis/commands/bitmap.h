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

#ifndef COKE_REDIS_COMMANDS_BITMAP_H
#define COKE_REDIS_COMMANDS_BITMAP_H

#include "coke/redis/basic_types.h"
#include "coke/redis/options.h"

namespace coke {

struct BitfieldBuilder {
    explicit BitfieldBuilder(StrHolder key)
    {
        args.push_back("BITFIELD"_sv);
        args.push_back(std::move(key));
    }

    /**
     * @brief Returns the specified bit field.
     *
     * Command: GET encoding offset
     */
    BitfieldBuilder &get(StrHolder encoding, StrHolder offset)
    {
        args.push_back("GET"_sv);
        args.push_back(std::move(encoding));
        args.push_back(std::move(offset));
        return *this;
    }

    /**
     * @brief Changes the behavior of successive INCRBY and SET subcommands.
     *
     * Command: OVERFLOW <WRAP | SAT | FAIL>
     */
    BitfieldBuilder &overflow(StrHolder type)
    {
        args.push_back("OVERFLOW"_sv);
        args.push_back(std::move(type));
        return *this;
    }

    /**
     * @brief Set the specified bit field and returns its old value.
     *
     * Command: SET encoding offset value
     */
    BitfieldBuilder &set(StrHolder encoding, StrHolder offset, int64_t value)
    {
        args.push_back("SET"_sv);
        args.push_back(std::move(encoding));
        args.push_back(std::move(offset));
        args.push_back(std::to_string(value));
        return *this;
    }

    /**
     * @brief Increments or decrements (if a negative increment is given) the
     *        specified bit field and returns the new value.
     *
     * Command: INCRBY encoding offset value
     */
    BitfieldBuilder &incrby(StrHolder encoding, StrHolder offset, int64_t value)
    {
        args.push_back("INCRBY"_sv);
        args.push_back(std::move(encoding));
        args.push_back(std::move(offset));
        args.push_back(std::to_string(value));
        return *this;
    }

    BitfieldBuilder move() { return std::move(*this); }

    StrHolderVec build() { return std::move(args); }

private:
    StrHolderVec args;
};

struct BitfieldRoBuilder {
    explicit BitfieldRoBuilder(StrHolder key)
    {
        args.push_back("BITFIELD_RO"_sv);
        args.push_back(std::move(key));
    }

    /**
     * @brief Returns the specified bit field.
     *
     * Command: GET encoding offset
     */
    BitfieldRoBuilder &get(StrHolder encoding, StrHolder offset)
    {
        args.push_back("GET"_sv);
        args.push_back(std::move(encoding));
        args.push_back(std::move(offset));
        return *this;
    }

    BitfieldRoBuilder move() { return std::move(*this); }

    StrHolderVec build() { return std::move(args); }

private:
    StrHolderVec args;
};

template<typename Client>
struct RedisBitmapCommands {
    /**
     * @brief Count the number of set bits in a string.
     * @return Integer reply, the number of bits set to 1.
     *
     * Command: BITCOUNT key [start end [BYTE | BIT]]
     *
     * Since Redis 2.6.0.
     * Since Redis 7.0.0, added the BYTE, BIT option.
     */
    auto bitcount(StrHolder key)
    {
        auto v = make_shv("BITCOUNT"_sv, std::move(key));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Count the number of set bits in a string.
     * @return Integer reply, the number of bits set to 1.
     *
     * Command: BITCOUNT key [start end [BYTE | BIT]]
     *
     * Since Redis 2.6.0.
     * Since Redis 7.0.0, added the BYTE, BIT option.
     */
    auto bitcount(StrHolder key, int32_t start, int32_t end, bool bit = false)
    {
        StrHolderVec v;

        v.reserve(4 + (bit ? 1 : 0));
        v.push_back("BITCOUNT"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(start));
        v.push_back(std::to_string(end));

        if (bit)
            v.push_back("BIT"_sv);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Operate redis string as array of bits.
     * @return Array reply or Nil/Null reply(if overflow fail).
     *
     * Command: BITFIELD key
     *          [GET encoding offset | [OVERFLOW <WRAP | SAT | FAIL>]
     *          <SET encoding offset value | INCRBY encoding offset increment>
     *          [GET encoding offset | [OVERFLOW <WRAP | SAT | FAIL>]
     *          <SET encoding offset value | INCRBY encoding offset increment>
     *          ...]]
     *
     * Since redis 3.2.0.
     */
    auto bitfield(BitfieldBuilder builder) { return run(builder.build()); }

    /**
     * @brief Operate redis string as array of bits.
     * @return Array reply or Nil/Null reply(if overflow fail).
     *
     * Command: BITFIELD_RO key [GET encoding offset [GET encoding offset ...]]
     *
     * Since redis 6.0.0.
     */
    auto bitfield_ro(BitfieldRoBuilder builder)
    {
        return run(builder.build(), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Perform bitwise operations between strings and store to `destkey`.
     * @return Integer reply, the size of the string stored in the `destkey`.
     *
     * Command: BITOP <AND | OR | XOR | NOT> destkey key [key ...]
     */
    auto bitop(StrHolder op, StrHolder destkey, StrHolderVec keys)
    {
        StrHolderVec v;

        v.reserve(3 + keys.size());
        v.push_back("BITOP"_sv);
        v.push_back(std::move(op));
        v.push_back(std::move(destkey));

        for (auto &key : keys)
            v.push_back(std::move(key));

        return run(std::move(v), {.slot = -2});
    }

    /**
     * @brief Find first bit set to `bit` in a string.
     * @return Integer reply.
     *
     * Command: BITPOS key value [start [end [BYTE | BIT]]]
     *
     * Since Redis 2.8.7.
     * Since Redis 7.0.0, added the BYTE, BIT option.
     */
    auto bitpos(StrHolder key, uint32_t bit, int32_t start = 0)
    {
        StrHolderVec v;

        v.reserve(4);
        v.push_back("BITPOS"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(bit));
        v.push_back(std::to_string(start));

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Find first bit set to `bit` in a string.
     * @return Integer reply.
     *
     * Command: BITPOS key value [start [end [BYTE | BIT]]]
     *
     * Since Redis 2.8.7.
     * Since Redis 7.0.0, added the BYTE, BIT option.
     */
    auto bitpos(StrHolder key, uint32_t value, int32_t start, int32_t end,
                bool bit = false)
    {
        StrHolderVec v;

        v.reserve(6);
        v.push_back("BITPOS"_sv);
        v.push_back(std::move(key));
        v.push_back(std::to_string(value));
        v.push_back(std::to_string(start));
        v.push_back(std::to_string(end));

        if (bit)
            v.push_back("BIT"_sv);

        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Get the bit value at `offset` in the string value.
     * @return Integer reply, 0 or 1.
     *
     * Command: GETBIT key offset
     */
    auto getbit(StrHolder key, uint32_t offset)
    {
        auto v = make_shv("GETBIT"_sv, std::move(key), std::to_string(offset));
        return run(std::move(v), {.flags = REDIS_FLAG_READONLY});
    }

    /**
     * @brief Sets or clears the bit at `offset` in the string value.
     * @return RESP2/RESP3 Integer reply, the original bit value.
     */
    auto setbit(StrHolder key, uint32_t offset, uint32_t bit)
    {
        auto v = make_shv("SETBIT"_sv, std::move(key), std::to_string(offset),
                          std::to_string(bit));
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

#endif // COKE_REDIS_COMMANDS_BITMAP_H
