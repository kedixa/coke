#ifndef COKE_SLEEP_H
#define COKE_SLEEP_H

#include <chrono>
#include <string>

#include "coke/detail/awaiter_base.h"

namespace coke {

constexpr int SLEEP_UNKNOWN_ERROR = -1;
constexpr int SLEEP_SUCCESS = 0;
constexpr int SLEEP_CANCELED = 1;
constexpr int SLEEP_ABORTED = 2;

class SleepAwaiter : public BasicAwaiter<int> {
    static std::chrono::nanoseconds __plus(long sec, long nsec) {
        using namespace std::chrono;
        return seconds(sec) + nanoseconds(nsec);
    }

public:
    SleepAwaiter() { emplace_result(SLEEP_SUCCESS); }
    SleepAwaiter(long sec, long nsec) : SleepAwaiter(__plus(sec, nsec)) { }

    SleepAwaiter(std::chrono::nanoseconds nsec);
    SleepAwaiter(const std::string &name, std::chrono::nanoseconds nsec);
};

inline SleepAwaiter sleep(long sec, long nsec) {
    return SleepAwaiter(sec, nsec);
}

template<typename Rep, typename Period>
SleepAwaiter sleep(const std::chrono::duration<Rep, Period> &d) {
    return SleepAwaiter(d);
}

template<typename Rep, typename Period>
SleepAwaiter sleep(const std::string &name, const std::chrono::duration<Rep, Period> &d) {
    return SleepAwaiter(name, d);
}

inline SleepAwaiter yield() {
    return SleepAwaiter(0, 0);
}

inline void cancel_sleep_by_name(const std::string &name, std::size_t max);

inline void cancel_sleep_by_name(const std::string &name) {
    return cancel_sleep_by_name(name, std::size_t(-1));
}

} // namespace coke

#endif // COKE_SLEEP_H
