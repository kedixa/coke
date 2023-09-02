#ifndef COKE_QPS_POOL_H
#define COKE_QPS_POOL_H

#include <mutex>

#include "coke/sleep.h"

namespace coke {

class QpsPool {
    using NanoType = long long;

public:
    using AwaiterType = SleepAwaiter;

public:
    /**
     * Create QpsPool with `qps` limit, `qps` == 0 means no limit.
     *  Example:
     *  coke::QpsPool pool(10);
     *
     *  // in coroutine, the following loop will take about 10 seconds
     *  for (size_t i = 0; i < 101; i++) {
     *      co_await pool.get();
     *      // do something under qps limit
     *  }
    */
    QpsPool(unsigned qps);

    /**
     * When qps is not integer, for example, 0.5 qps means 2 seconds
     * per query. `qps` <= 0.0 means no limit, invalid param may cause
     * bad result.
    */
    QpsPool(double qps);

    /**
     * Reset another `qps` limit, even if pool is already in use.
     * The new limit will take effect the next time `get` is called.
    */
    void reset_qps(unsigned qps) noexcept;
    void reset_qps(double qps) noexcept;

    /**
     * Acquire `count` license from QpsPool, the returned awaitable
     * object should be co awaited immediately.
    */
    AwaiterType get(unsigned count = 1) noexcept;

private:
    std::mutex mtx;
    NanoType interval_nano;
    NanoType last_nano;
};

} // namespace coke

#endif // COKE_QPS_POOL_H
