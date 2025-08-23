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

#ifndef COKE_REDIS_OPTIONS_H
#define COKE_REDIS_OPTIONS_H

#include <concepts>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "coke/utils/str_holder.h"

namespace coke {

template<typename... Opts>
struct RedisOptsChoice {
    using OptsVariant = std::variant<Opts...>;

    RedisOptsChoice()
        requires std::is_default_constructible_v<OptsVariant>
    {
    }

    template<typename T>
        requires std::constructible_from<OptsVariant, T>
    RedisOptsChoice(const T &opt) : opt(opt)
    {
    }

    template<typename T>
        requires std::assignable_from<OptsVariant, T>
    RedisOptsChoice &operator=(const T &opt)
    {
        this->opt = opt;
        return *this;
    }

    void append_to(StrHolderVec &vec) const
    {
        std::visit([&](const auto &arg) { arg.append_to(vec); }, opt);
    }

    std::size_t size() const
    {
        return std::visit([](const auto &arg) { return arg.size(); }, opt);
    }

private:
    OptsVariant opt;
};

struct RedisOptNone {
    constexpr std::size_t size() const noexcept { return 0; }
    void append_to(StrHolderVec &) const {}
};

template<typename T>
struct RedisOptStrBase {
    constexpr std::size_t size() const noexcept { return 1; }
    void append_to(StrHolderVec &vec) const { vec.push_back(T::str); }
};

struct RedisOptEx {
    int64_t seconds;

    constexpr std::size_t size() const noexcept { return 2; }

    void append_to(StrHolderVec &vec) const
    {
        vec.push_back("EX"_sv);
        vec.push_back(std::to_string(seconds));
    }
};

struct RedisOptPx {
    int64_t milliseconds;

    constexpr std::size_t size() const noexcept { return 2; }

    void append_to(StrHolderVec &vec) const
    {
        vec.push_back("PX"_sv);
        vec.push_back(std::to_string(milliseconds));
    }
};

struct RedisOptExat {
    int64_t timestamp_seconds;

    constexpr std::size_t size() const noexcept { return 2; }

    void append_to(StrHolderVec &vec) const
    {
        vec.emplace_back("EXAT"_sv);
        vec.emplace_back(std::to_string(timestamp_seconds));
    }
};

struct RedisOptPxat {
    int64_t timestamp_milliseconds;

    constexpr std::size_t size() const noexcept { return 2; }

    void append_to(StrHolderVec &vec) const
    {
        vec.emplace_back("PXAT"_sv);
        vec.emplace_back(std::to_string(timestamp_milliseconds));
    }
};

struct RedisOptPersist : public RedisOptStrBase<RedisOptPersist> {
    constexpr static std::string_view str = "PERSIST";
};

struct RedisOptKeepttl : public RedisOptStrBase<RedisOptKeepttl> {
    constexpr static std::string_view str = "KEEPTTL";
};

struct RedisOptNx : public RedisOptStrBase<RedisOptNx> {
    constexpr static std::string_view str = "NX";
};

struct RedisOptXx : public RedisOptStrBase<RedisOptXx> {
    constexpr static std::string_view str = "XX";
};

struct RedisOptGt : public RedisOptStrBase<RedisOptGt> {
    constexpr static std::string_view str = "GT";
};

struct RedisOptLt : public RedisOptStrBase<RedisOptLt> {
    constexpr static std::string_view str = "LT";
};

struct RedisOptFnx : public RedisOptStrBase<RedisOptFnx> {
    constexpr static std::string_view str = "FNX";
};

struct RedisOptFxx : public RedisOptStrBase<RedisOptFxx> {
    constexpr static std::string_view str = "FXX";
};

struct RedisOptLeft : public RedisOptStrBase<RedisOptLeft> {
    constexpr static std::string_view str = "LEFT";
};

struct RedisOptRight : public RedisOptStrBase<RedisOptRight> {
    constexpr static std::string_view str = "RIGHT";
};

struct RedisOptBefore : public RedisOptStrBase<RedisOptBefore> {
    constexpr static std::string_view str = "BEFORE";
};

struct RedisOptAfter : public RedisOptStrBase<RedisOptAfter> {
    constexpr static std::string_view str = "AFTER";
};

using RedisSetExpireOpt =
    RedisOptsChoice<RedisOptNone, RedisOptEx, RedisOptPx, RedisOptExat,
                    RedisOptPxat, RedisOptKeepttl>;

using RedisExpireOpt = RedisOptsChoice<RedisOptNone, RedisOptNx, RedisOptXx,
                                       RedisOptGt, RedisOptLt>;

} // namespace coke

#endif // COKE_REDIS_OPTIONS_H
