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

#include "coke/utils/str_packer.h"

#include <algorithm>
#include <functional>

namespace coke {

void StrPacker::do_merge_all()
{
    std::size_t size = 0;

    for (const auto &sh : strs)
        size += sh.size();

    std::string str;
    str.reserve(size);

    for (const auto &sh : strs)
        str.append(sh.as_view());

    std::vector<StrHolder> tmp;
    tmp.push_back(std::move(str));
    tmp.swap(strs);
}

void StrPacker::do_merge(std::size_t m)
{
    std::vector<std::size_t> vlen;
    vlen.reserve(strs.size());

    for (const auto &sh : strs)
        vlen.push_back(sh.size());

    auto cmp = std::greater<std::size_t>{};
    std::nth_element(vlen.begin(), vlen.begin() + m - 1, vlen.end(), cmp);

    std::size_t sp = vlen[m - 1];
    std::size_t nsp = 0;

    for (std::size_t i = 0; i < m; i++) {
        if (vlen[i] == sp)
            ++nsp;
    }

    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t size = 0;
    std::size_t last_size;

    std::vector<StrHolder> merged;
    merged.reserve(m * 2 + 1);

    auto merge_range = [&]() {
        if (last - first == 1) {
            merged.push_back(std::move(strs[first]));
            ++first;
            size = 0;
        }
        else {
            std::string tmp;
            tmp.reserve(size);

            while (first < last) {
                tmp.append(strs[first].as_view());
                ++first;
            }

            merged.emplace_back(std::move(tmp));
            size = 0;
        }
    };

    while (last < strs.size()) {
        bool need_merge = false;

        last_size = strs[last].size();
        if (last_size > sp) {
            need_merge = true;
        }
        else if (last_size == sp && nsp > 0) {
            need_merge = true;
            --nsp;
        }

        if (!need_merge) {
            size += last_size;
            ++last;
            continue;
        }

        if (first < last)
            merge_range();

        merged.push_back(std::move(strs[last]));
        ++last;
        first = last;
    }

    if (first < last)
        merge_range();

    merged.swap(strs);
}

} // namespace coke
