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

#ifndef COKE_TASK_H
#define COKE_TASK_H

#include "coke/detail/task_impl.h"

namespace coke {

using detail::Task;

/**
 * @brief Check whether T is coke::Task type.
 */
template<typename T>
constexpr inline bool is_task_v = detail::TaskHelper<T>::value;

/**
 * If T is coke::Task<U>, then TaskRetType<T> is U
 */
template<typename T>
using TaskRetType = typename detail::TaskHelper<T>::RetType;

/**
 * @brief Detach the coke::Task. Each valid coke::Task can be co awaited,
 *        detached, detached on series, detached on new series at most once.
 * @param task Valid coke::Task object.
 */
template<Cokeable T>
void detach(Task<T> &&task)
{
    task.detach();
}

} // namespace coke

#endif // COKE_TASK_H
