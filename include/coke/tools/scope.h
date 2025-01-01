/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
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

#ifndef COKE_TOOLS_SCOPE_H
#define COKE_TOOLS_SCOPE_H

#include <type_traits>

namespace coke {

template<typename F>
    requires (std::is_nothrow_invocable_r_v<void, F>
              && std::is_nothrow_move_constructible_v<F>)
class ScopeExit {
public:
    explicit ScopeExit(F f)
        : func(std::move(f))
    { }

    ScopeExit(const ScopeExit &) = delete;
    ScopeExit &operator=(const ScopeExit &) = delete;

    void release() noexcept {
        active = false;
    }

    ~ScopeExit() {
        if (active)
            func();
    }

private:
    [[no_unique_address]] F func;
    bool active{true};
};

template<typename F>
ScopeExit(F) -> ScopeExit<F>;

} // namespace coke

#endif // COKE_TOOLS_SCOPE_H
