#include <chrono>

#include "coke/qps_pool.h"

namespace coke {

static long long __get_current_nano() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

QpsPool::QpsPool(unsigned qps) {
    last_nano = 0;

    if (qps == 0)
        interval_nano = 0;
    else
        interval_nano = 1000000000ULL / qps;
}

SleepAwaiter QpsPool::get(unsigned cnt) {
    std::lock_guard<std::mutex> lg(mtx);
    nano_type current, cost, next;

    current = __get_current_nano();
    cost = interval_nano * cnt;
    next = last_nano + cost;

    if (next >= current) {
        last_nano = next;
        return SleepAwaiter(0, next-current);
    }
    else {
        last_nano = current;
        return SleepAwaiter();
    }
}

} // namespace coke
