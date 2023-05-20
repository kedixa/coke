#ifndef COKE_SLEEP_H
#define COKE_SLEEP_H

#include <chrono>

#include "coke/detail/awaiter_base.h"

namespace coke {

class SleepAwaiter : public AwaiterBase {
public:
    SleepAwaiter() : state(0) { }
    SleepAwaiter(long sec, long nsec);

    // may return STATE_SUCCESS or STATE_ABORTED
    int await_resume() { return state; }

private:
    int state;
};

inline SleepAwaiter sleep(long sec, long nsec) {
    return SleepAwaiter(sec, nsec);
}

inline SleepAwaiter sleep(std::chrono::nanoseconds nsec) {
    constexpr size_t NANO = 1000ULL * 1000 * 1000;
    std::chrono::nanoseconds::rep cnt = nsec.count();
    return SleepAwaiter(cnt / NANO, cnt % NANO);
}

inline SleepAwaiter yield() {
    return SleepAwaiter(0, 0);
}

} // namespace coke

#endif // COKE_SLEEP_H
