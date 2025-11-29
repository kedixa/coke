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

#ifndef COKE_QPS_POOL_H
#define COKE_QPS_POOL_H

#include <cstdint>
#include <mutex>

#include "coke/sleep.h"

namespace coke {

class QpsPool {
    using NanoType = int64_t;
    static constexpr NanoType SUB_MASK = 1'000L;

public:
    using AwaiterType = SleepAwaiter;

public:
    /**
     * @brief Create QpsPool with `query` query in `seconds` seconds.
     *        Requires `query` >= 0 and seconds >= 1.
     */
    QpsPool(long query, long seconds = 1);

    /**
     * @brief Reset another `qps` limit, even if pool is already in use.
     *        The new limit will take effect the next time `get` is called.
     */
    void reset_qps(long query, long seconds = 1);

    /**
     * @brief Acquire `count` license from QpsPool.
     * @return An awaitable object that should be co awaited immediately.
     */
    AwaiterType get(unsigned count = 1)
    {
        return get_if(count, NanoSec::max());
    }

    /**
     * @brief Acquire `count` license from QpsPool if wait period <= `nsec`.
     * @return An awaitable object that should be co awaited immediately.
     * @retval coke::SLEEP_SUCCESS if the license if acquired.
     * @retval coke::SLEEP_CANCELED if cannot be acquired in nsec.
     */
    AwaiterType get_if(unsigned count, NanoSec nsec);

private:
    std::mutex mtx;
    NanoType interval_nano;
    NanoType interval_sub;
    NanoType last_nano;
    NanoType last_sub;
};

} // namespace coke

#endif // COKE_QPS_POOL_H
