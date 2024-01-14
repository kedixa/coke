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
    AwaiterType get(unsigned count = 1) noexcept;

private:
    std::mutex mtx;
    NanoType interval_nano;
    NanoType interval_sub;
    NanoType last_nano;
    NanoType last_sub;
};

} // namespace coke

#endif // COKE_QPS_POOL_H
