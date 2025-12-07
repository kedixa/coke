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

#ifndef COKE_MAKE_TASK_H
#define COKE_MAKE_TASK_H

#include <functional>
#include <memory>

#include "coke/task.h"

namespace coke {

template<typename FUNC, typename... ARGS>
    requires std::invocable<FUNC, ARGS...>
auto make_task(FUNC &&func, ARGS &&...args)
{
    using result_t = std::invoke_result_t<FUNC, ARGS...>;
    static_assert(is_task_v<result_t>,
                  "FUNC(ARGS...) must return coke::Task<T>");

    result_t res;
    if constexpr (std::is_rvalue_reference_v<FUNC &&>) {
        std::shared_ptr<FUNC> ctx = std::make_shared<FUNC>(
            std::forward<FUNC>(func));
        res = std::invoke(*ctx, std::forward<ARGS>(args)...);
        res.set_context(ctx);
    }
    else {
        res = std::invoke(std::forward<FUNC>(func),
                          std::forward<ARGS>(args)...);
    }

    return res;
}

} // namespace coke

#endif // COKE_MAKE_TASK_H
