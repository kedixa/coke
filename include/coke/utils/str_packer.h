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

#ifndef COKE_UTILS_STR_PACKER_H
#define COKE_UTILS_STR_PACKER_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "coke/utils/str_holder.h"

namespace coke {

class StrPacker {
    constexpr static std::size_t SMALL_HINT = 128;
    constexpr static std::size_t BLOCK_HINT = 16 * 1024;

public:
    StrPacker() = default;

    StrPacker(StrPacker &&p) noexcept : strs(std::move(p.strs)) { p.clear(); }

    StrPacker &operator=(StrPacker &&p) noexcept
    {
        if (this != &p) {
            strs = std::move(p.strs);
            p.clear();
        }

        return *this;
    }

    StrPacker(const StrPacker &) = delete;
    StrPacker &operator=(const StrPacker &) = delete;

    ~StrPacker() = default;

    StrPacker &append(const char *str)
    {
        return append_impl(str, std::strlen(str));
    }

    StrPacker &append(const std::string &str)
    {
        return append_impl(str.data(), str.size());
    }

    StrPacker &append(std::string_view sv)
    {
        return append_impl(sv.data(), sv.size());
    }

    StrPacker &append(const char *str, std::size_t len)
    {
        return append_impl(str, len);
    }

    StrPacker &append(std::string &&str)
    {
        return append_nocopy_impl(std::move(str));
    }

    StrPacker &append_nocopy(const char *str)
    {
        return append_nocopy_impl(str, std::strlen(str));
    }

    StrPacker &append_nocopy(const std::string &str)
    {
        return append_nocopy_impl(str.data(), str.size());
    }

    StrPacker &append_nocopy(std::string_view sv)
    {
        return append_nocopy_impl(sv.data(), sv.size());
    }

    StrPacker &append_nocopy(const char *str, std::size_t len)
    {
        return append_nocopy_impl(str, len);
    }

    StrPacker &append_nocopy(std::string &&str)
    {
        return append_nocopy_impl(std::move(str));
    }

    StrPacker &extend_copy(const StrPacker &pk)
    {
        for (const StrHolder &sh : pk.strs)
            append(sh.as_view());

        return *this;
    }

    StrPacker &extend_nocopy(StrPacker &&pk)
    {
        for (StrHolder &sh : pk.strs) {
            if (sh.holds_view())
                append_nocopy(sh.get_view());
            else
                append_nocopy(std::move(sh.get_string()));
        }

        pk.clear();
        return *this;
    }

    /**
     * @brief Merge the internal strs.
     * @param max max should >= 1. The max number of strs after merging.
     */
    void merge(std::size_t max)
    {
        if (strs.size() <= max)
            return;

        if (max <= 2)
            return do_merge_all();

        return do_merge((max - 1) / 2);
    }

    std::size_t strs_count() const noexcept { return strs.size(); }

    /**
     * @brief Get the total bytes of all strs.
     * @attention The complexity is O(n).
     */
    std::size_t total_bytes() const noexcept
    {
        std::size_t n = 0;

        for (const auto &sh : strs)
            n += sh.size();

        return n;
    }

    /**
     * @brief Get the internal strs.
     * @attention If the strs are modified, clear() should be called to reset
     *            the internal state before further usage.
     */
    std::vector<StrHolder> &get_strs() noexcept { return strs; }

    const std::vector<StrHolder> &get_strs() const noexcept { return strs; }

    /**
     * @brief Clear the internal strs.
     */
    void clear() { strs.clear(); }

private:
    StrPacker &append_impl(const char *str, std::size_t len)
    {
        if (len > 0)
            buf(len).append(str, len);

        return *this;
    }

    StrPacker &append_nocopy_impl(const char *str, std::size_t len)
    {
        if (len <= SMALL_HINT)
            append_impl(str, len);
        else
            strs.emplace_back(std::string_view(str, len));

        return *this;
    }

    StrPacker &append_nocopy_impl(std::string &&str)
    {
        if (str.size() <= SMALL_HINT)
            append_impl(str.data(), str.size());
        else
            strs.emplace_back(std::move(str));

        return *this;
    }

    std::string &buf(std::size_t hint)
    {
        if (strs.empty() || strs.back().holds_view()) {
            strs.push_back(std::string{});
        }
        else {
            std::string &s = strs.back().get_string();
            if (s.size() > BLOCK_HINT && s.capacity() - s.size() < hint)
                strs.push_back(std::string{});
        }

        return strs.back().get_string();
    }

    void do_merge_all();

    void do_merge(std::size_t m);

private:
    std::vector<StrHolder> strs;
};

} // namespace coke

#endif // COKE_UTILS_STR_PACKER_H
