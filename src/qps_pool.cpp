#include <chrono>
#include <cmath>
#include <cassert>

#include "coke/qps_pool.h"

namespace coke {

static int64_t __get_current_nano() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

QpsPool::QpsPool(long query, long seconds) {
    last_nano = 0;
    last_sub = 0;

    reset_qps(query, seconds);
}

void QpsPool::reset_qps(long query, long seconds) noexcept {
    assert(query >= 0 && seconds >= 1);

    std::lock_guard<std::mutex> lg(mtx);

    if (query == 0) {
        interval_nano = 0;
        interval_sub = 0;
    }
    else {
        double d = 1e9 * seconds / query;
        double f = std::floor(d);
        d -= f;

        interval_nano = NanoType(f);
        interval_sub = NanoType(d * SUB_MASK);
    }
}

QpsPool::AwaiterType QpsPool::get(unsigned count) noexcept {
    std::lock_guard<std::mutex> lg(mtx);
    NanoType current, next_nano, next_sub;

    next_nano = last_nano + interval_nano * count;
    next_sub = last_sub + interval_sub * count;

    next_nano += next_sub / SUB_MASK;
    next_sub %= SUB_MASK;

    current = __get_current_nano();

    if (next_nano > current) {
        last_nano = next_nano;
        last_sub = next_sub;

        return AwaiterType(std::chrono::nanoseconds(next_nano - current));
    }
    else {
        last_nano = current;
        last_sub = 0;
        return AwaiterType();
    }
}

} // namespace coke
