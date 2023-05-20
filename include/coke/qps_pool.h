#ifndef COKE_QPS_POOL_H
#define COKE_QPS_POOL_H

#include <mutex>

#include "coke/sleep.h"

namespace coke {

class QpsPool {
    using nano_type = long long;

public:
    QpsPool(unsigned qps);

    SleepAwaiter get(unsigned cnt = 1);

private:
    std::mutex mtx;
    nano_type interval_nano;
    nano_type last_nano;
};

} // namespace coke

#endif // COKE_QPS_POOL_H
