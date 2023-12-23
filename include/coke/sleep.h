#ifndef COKE_SLEEP_H
#define COKE_SLEEP_H

#include <chrono>
#include <string>

#include "coke/global.h"

namespace coke {

// Negative numbers indicate errors
constexpr int SLEEP_SUCCESS = 0;
constexpr int SLEEP_CANCELED = 1;
constexpr int SLEEP_ABORTED = 2;

static_assert(SLEEP_SUCCESS == TOP_SUCCESS);
static_assert(SLEEP_ABORTED == TOP_ABORTED);


/**
 * InfiniteDuration is used to represent an infinite time when sleeping with id,
 * and will only be awakened when `cancel_sleep_by_id`.
*/
struct InfiniteDuration { };


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
    SleepAwaiter(uint64_t id, std::chrono::nanoseconds nsec,
                 bool insert_head=false);
    SleepAwaiter(uint64_t id, InfiniteDuration, bool insert_head=false);
};


inline SleepAwaiter sleep(long sec, long nsec) {
    return SleepAwaiter(sec, nsec);
}

template<typename Rep, typename Period>
SleepAwaiter sleep(const std::chrono::duration<Rep, Period> &duration) {
    return SleepAwaiter(duration);
}

template<typename Rep, typename Period>
SleepAwaiter sleep(const std::string &name,
                   const std::chrono::duration<Rep, Period> &duration) {
    return SleepAwaiter(name, duration);
}

template<typename Rep, typename Period>
SleepAwaiter sleep(uint64_t id,
                   const std::chrono::duration<Rep, Period> &duration,
                   bool insert_head = false)
{
    return SleepAwaiter(id, duration, insert_head);
}

inline SleepAwaiter sleep(uint64_t id, const InfiniteDuration &inf_duration,
                          bool insert_head = false)
{
    return SleepAwaiter(id, inf_duration, insert_head);
}

inline SleepAwaiter yield() {
    return SleepAwaiter(0, 0);
}

void cancel_sleep_by_name(const std::string &name, std::size_t max);

inline void cancel_sleep_by_name(const std::string &name) {
    return cancel_sleep_by_name(name, std::size_t(-1));
}

std::size_t cancel_sleep_by_id(uint64_t id, std::size_t max);

inline std::size_t cancel_sleep_by_id(uint64_t id) {
    return cancel_sleep_by_id(id, std::size_t(-1));
}

} // namespace coke

#endif // COKE_SLEEP_H
