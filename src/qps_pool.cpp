#include <chrono>
#include <cmath>

#include "coke/qps_pool.h"

namespace coke {

static long long __get_current_nano() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

QpsPool::QpsPool(unsigned qps) {
    last_nano = 0;

    reset_qps(qps);
}

QpsPool::QpsPool(double qps) {
    last_nano = 0;

    reset_qps(qps);
}

void QpsPool::reset_qps(unsigned qps) noexcept {
    std::lock_guard<std::mutex> lg(mtx);

    if (qps == 0)
        interval_nano = 0;
    else
        interval_nano = 1000000000ULL / qps;
}

void QpsPool::reset_qps(double qps) noexcept {
    std::lock_guard<std::mutex> lg(mtx);

    if (qps <= 0.0 || !std::isfinite(qps))
        interval_nano = 0;
    else
        interval_nano = 1.0e9 / qps;
}

QpsPool::AwaiterType QpsPool::get(unsigned count) noexcept {
    std::lock_guard<std::mutex> lg(mtx);
    NanoType current, cost, next;

    current = __get_current_nano();
    cost = interval_nano * count;
    next = last_nano + cost;

    if (next >= current) {
        last_nano = next;
        return AwaiterType(0, next - current);
    }
    else {
        last_nano = current;
        return AwaiterType();
    }
}

} // namespace coke
