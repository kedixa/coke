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

#include <mutex>
#include <cstdint>

#include "coke/sleep.h"

namespace coke {

class QpsPool {
    using NanoType = int64_t;
    static constexpr NanoType SUB_MASK = 1'000L;

public:
    using AwaiterType = SleepAwaiter;

public:
    /**
     * Create QpsPool with query/seconds limit, `query` == 0 means no limit.
     * requires `query` >= 0 and seconds >= 1
     *  Example:
     *  coke::QpsPool pool(10);
     *
     *  // in coroutine, the following loop will take about 10 seconds
     *  for (std::size_t i = 0; i < 101; i++) {
     *      co_await pool.get();
     *      // do something under qps limit
     *  }
    */
    QpsPool(long query, long seconds = 1);

    /**
     * Reset another `qps` limit, even if pool is already in use.
     * The new limit will take effect the next time `get` is called.
    */
    void reset_qps(long query, long seconds = 1) noexcept;

    /**
     * Acquire `count` license from QpsPool, the returned awaitable
     * object should be co awaited immediately.
    */
    [[nodiscard]]
    AwaiterType get(unsigned count = 1) noexcept {
        return get_if(count, NanoSec::max());
    }

    /**
     * Acquire `count` license from QpsPool when the wait period less or equal
     * to `ns`, the returned awaitable object should be co awaited immediately.
     * The awaiter returns `coke::SLEEP_CANCELED` immediately if the license
     * cannot be acquired in `ns`.
    */
    [[nodiscard]]
    AwaiterType get_if(unsigned count, const NanoSec &nsec) noexcept;

private:
    std::mutex mtx;
    NanoType interval_nano;
    NanoType interval_sub;
    NanoType last_nano;
    NanoType last_sub;
};

} // namespace coke

#endif // COKE_QPS_POOL_H
