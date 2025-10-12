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

#ifndef COKE_DETAIL_CONDITION_IMPL_H
#define COKE_DETAIL_CONDITION_IMPL_H

#include <functional>
#include <mutex>

#include "coke/sleep.h"
#include "coke/task.h"

namespace coke::detail {

Task<int> cv_wait(std::unique_lock<std::mutex> &lock, const void *addr,
                  TimedWaitHelper helper, int *wait_cnt);

Task<int> cv_wait(std::unique_lock<std::mutex> &lock, const void *addr,
                  TimedWaitHelper helper, std::function<bool()> pred,
                  int *wait_cnt);

inline Task<int> cv_wait(std::unique_lock<std::mutex> &lock, const void *addr,
                         TimedWaitHelper helper)
{
    return cv_wait(lock, addr, helper, nullptr);
}

inline Task<int> cv_wait(std::unique_lock<std::mutex> &lock, const void *addr,
                         TimedWaitHelper helper, std::function<bool()> pred)
{
    return cv_wait(lock, addr, helper, pred, nullptr);
}

inline std::size_t cv_notify(const void *addr, std::size_t n = std::size_t(-1))
{
    return cancel_sleep_by_addr(addr, n);
}

} // namespace coke::detail

#endif // COKE_DETAIL_CONDITION_IMPL_H
