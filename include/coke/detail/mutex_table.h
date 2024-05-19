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

#ifndef COKE_DETAIL_MUTEX_TABLE_H
#define COKE_DETAIL_MUTEX_TABLE_H

#include <mutex>

namespace coke::detail {

/**
 * @brief Provides a mutex table for internal use only.
 *
 * @param ptr The same ptr obtains same mutex, `this` pointer is always used.
 * @return The reference of std::mutex.
*/
std::mutex &get_mutex(void *ptr);

} // namespace coke::detail

#endif // COKE_DETAIL_MUTEX_TABLE_H
