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

#ifndef COKE_UTILS_STR_HOLDER_H
#define COKE_UTILS_STR_HOLDER_H

#include <concepts>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace coke {

class StrHolder {
public:
    StrHolder() noexcept : value(std::string()) {}

    StrHolder(std::string_view sv) noexcept : value(sv) {}

    StrHolder(std::string &&s) noexcept : value(std::move(s)) {}

    StrHolder(const std::string &s) : value(s) {}

    StrHolder(const char *s) : value(std::in_place_type_t<std::string>(), s) {}

    StrHolder(const StrHolder &other) = default;
    StrHolder(StrHolder &&other) noexcept = default;

    ~StrHolder() = default;

    StrHolder &operator=(const StrHolder &other) = default;
    StrHolder &operator=(StrHolder &&other) noexcept = default;

    StrHolder &operator=(const std::string_view &sv) noexcept
    {
        value = sv;
        return *this;
    }

    StrHolder &operator=(std::string &&s) noexcept
    {
        value = std::move(s);
        return *this;
    }

    StrHolder &operator=(const std::string &s)
    {
        value = s;
        return *this;
    }

    StrHolder &operator=(const char *s)
    {
        value.emplace<std::string>(s);
        return *this;
    }

    bool holds_string() const noexcept
    {
        return std::holds_alternative<std::string>(value);
    }

    bool holds_view() const noexcept
    {
        return std::holds_alternative<std::string_view>(value);
    }

    std::string_view get_view() const
    {
        return std::get<std::string_view>(value);
    }

    const std::string &get_string() const &
    {
        return std::get<std::string>(value);
    }

    std::string &get_string() & { return std::get<std::string>(value); }

    std::string get_string() &&
    {
        return std::get<std::string>(std::move(value));
    }

    std::string_view as_view() const & noexcept
    {
        if (holds_view())
            return get_view();
        else
            return get_string();
    }

    std::size_t size() const
    {
        if (holds_view())
            return get_view().size();
        else
            return get_string().size();
    }

private:
    std::variant<std::string, std::string_view> value;
};

template<typename T>
concept IndirectlyToStr =
    std::indirectly_readable<T> &&
    std::constructible_from<StrHolder, std::iter_value_t<T>>;

using StrHolderVec = std::vector<StrHolder>;

inline auto operator""_sv(const char *str, std::size_t len)
{
    return std::string_view(str, len);
}

inline auto operator""_svh(const char *str, std::size_t len)
{
    return StrHolder(std::string_view(str, len));
}

/**
 * @brief Make StrHolderVec from strs.
 *
 * ```cpp
 * std::string str;
 * std::string_view sv;
 * auto vec = coke::make_shv("hello", str, sv);
 * ```
 */
template<typename... Strs>
    requires (std::constructible_from<StrHolder, Strs &&> && ...)
StrHolderVec make_shv(Strs &&...strs)
{
    StrHolderVec v;
    v.reserve(sizeof...(strs));
    (v.emplace_back(std::forward<Strs>(strs)), ...);
    return v;
}

/**
 * @brief Make StrHolderVec from iterator range [begin, end).
 *
 * ```cpp
 * std::vector<std::string> vstr = ...;
 * auto vec = coke::make_shv(vstr.begin(), vstr.end());
 * ```
 */
template<IndirectlyToStr BeginIt, IndirectlyToStr EndIt>
StrHolderVec make_shv(BeginIt begin, EndIt end)
{
    StrHolderVec v;

    if constexpr (std::random_access_iterator<BeginIt> &&
                  std::random_access_iterator<EndIt>)
    {
        v.reserve(std::distance(begin, end));
    }

    v.assign(begin, end);
    return v;
}

/**
 * @brief Make StrHolderVec from iterator range [begin, end), store strings
 *        as std::string_view.
 *
 * ```cpp
 * std::vector<std::string> vstr = ...;
 * auto vec = coke::make_shv_view(vstr.begin(), vstr.end());
 * ```
 */
template<IndirectlyToStr BeginIt, IndirectlyToStr EndIt>
StrHolderVec make_shv_view(BeginIt begin, EndIt end)
{
    StrHolderVec v;

    if constexpr (std::random_access_iterator<BeginIt> &&
                  std::random_access_iterator<EndIt>)
    {
        v.reserve(std::distance(begin, end));
    }

    for (auto it = begin; it != end; ++it)
        v.push_back(std::string_view(*it));

    return v;
}

} // namespace coke

#endif // COKE_UTILS_STR_HOLDER_H
